#include "ServerInternal.h"

#include <atomic>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace unistdpp;

unistdpp::FD
acquireServerLock(const char* path) {
  // unistdpp::open only wraps the 2-arg (path, flags) form of ::open, so
  // O_CREAT's mode argument needs the raw POSIX call here, same as
  // swtcon's own create_pid_file() (libs/swtcon/init.cpp).
  FD fd{ ::open(path, O_RDWR | O_CREAT, 0666) };
  if (!fd.isValid()) {
    perror("Failed to open server lock file");
    std::exit(EXIT_FAILURE);
  }
  if (flock(fd.fd, LOCK_EX | LOCK_NB) != 0) {
    std::cerr << "Another rm2fb-server instance is already running\n";
    std::exit(EXIT_FAILURE);
  }
  return fd;
}

Result<FD>
createTCPSocket(int port) {
  auto listenfd = TRY(unistdpp::socket(AF_INET, SOCK_STREAM, 0));

  // lose the pesky "Address already in use" error message
  int yes = 1;
  TRY(unistdpp::setsockopt(
    listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

  TRY(unistdpp::bind(listenfd, Address::fromHostPort(INADDR_ANY, port)));
  TRY(unistdpp::listen(listenfd, 1));

  return listenfd;
}

unistdpp::Result<std::string>
getProcName(pid_t pid) {
  auto fd = TRY(unistdpp::open(
    (std::filesystem::path("/proc") / std::to_string(pid) / "cmdline").c_str(),
    O_RDONLY));

  std::array<char, 512> buf;
  auto size = TRY(fd.readAll(buf.data(), buf.size()));

  // cmdline is a NUL-separated argv[] list (e.g. "xochitl\0--system\0" for
  // the real xochitl) - only argv[0] matters here. std::filesystem::path
  // treats an embedded NUL as an ordinary character, not a terminator, so
  // without this the whole "xochitl\0--system" would get parsed as one
  // filename component instead (used only for getClients()'s display
  // name).
  auto argv0Size = std::find(buf.data(), buf.data() + size, '\0') - buf.data();
  auto res = std::string(buf.data(), argv0Size);

  return std::filesystem::path(res).filename().string();
}

bool
checkDebugMode() {
  const static bool debug_mode = [] {
    const auto* debugEnv = getenv("RM2FB_DEBUG");
    return debugEnv != nullptr && debugEnv != std::string_view("0");
  }();
  if (debug_mode) {
    std::cerr << "Debug Mode!\n";
  }
  return debug_mode;
}

bool
doTCPUpdate(unistdpp::FD& fd, const SharedFB& fb, const UpdateParams& params) {
  if (auto res = fd.writeAll(params); !res) {
    std::cerr << "Error writing: " << to_string(res.error()) << "\n";
    fd.close();
    return false;
  }

  int width = params.x2 - params.x1 + 1;
  int height = params.y2 - params.y1 + 1;
  int size = width * height;
  std::vector<uint16_t> buffer(size);

  for (int row = 0; row < height; row++) {
    int fbRow = row + params.y1;

    memcpy(&buffer[row * width],
           // NOLINTNEXTLINE
           static_cast<uint16_t*>(fb.getFb()) + fbRow * fb_width + params.x1,
           width * sizeof(uint16_t));
  }

  const auto writeSize = size * sizeof(uint16_t);
  auto res = fd.writeAll(buffer.data(), writeSize);
  if (!res) {
    std::cerr << "Error writing: " << to_string(res.error()) << "\n";
    fd.close();
    return false;
  }

  return true;
}

void
handleMsg(const SharedFB& fb,
          unistdpp::FD& fd,
          const AllUinputDevices& devs,
          GetUpdate msg) {
  UpdateParams params{
    .y1 = 0,
    .x1 = 0,
    .y2 = fb_height - 1,
    .x2 = fb_width - 1,
    .flags = 0,
    .waveform = 0,
    .temperatureOverride = 0,
    .extraMode = 0,
  };
  doTCPUpdate(fd, fb, params);
}

void
handleMsg(const SharedFB& fb,
          unistdpp::FD& fd,
          const AllUinputDevices& devs,
          const Input& msg) {
  if (!msg.touch && devs.wacom) {
    sendPen(msg, *devs.wacom);
  }
  if (msg.touch && devs.touch) {
    sendTouch(msg, *devs.touch);
  }
}

void
handleMsg(const SharedFB& fb,
          unistdpp::FD& fd,
          const AllUinputDevices& devs,
          const PowerButton& msg) {
  if (devs.button) {
    sendButton(msg.down, *devs.button);
  }
}

pid_t
focusPid(const FocusState& focus) {
  return std::visit(
    [](const auto& state) -> pid_t {
      using T = std::decay_t<decltype(state)>;
      if constexpr (std::is_same_v<T, NoFront>) {
        return 0;
      } else {
        return state.pid;
      }
    },
    focus);
}

namespace {

std::atomic_bool running = true; // NOLINT

void
onSigint(int num) {
  running = false;
}

void
setupExitHandler() {
  struct sigaction action{};

  action.sa_handler = onSigint;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGINT, &action, nullptr) == -1) {
    perror("Sigaction");
    exit(EXIT_FAILURE);
  }
}

void
setProcessName(char* argv0) {
  if (argv0 == std::string_view("/usr/bin/xochitl")) {
    strcpy(argv0, "__rm2fb_server__");
  }
}

} // namespace

int
serverMain(char* argv0, const AddressInfoBase* addrs) { // NOLINT
  umask(0);

  setupExitHandler();
  setProcessName(argv0);

  Server server(addrs);
  server.initSWTCON();

  std::cerr << "rm2fb-server started!\n";
  sd_notify(0, "READY=1");

  while (running) {
    if (server.poll()) {
      std::cerr << "Unix clients: " << server.unixClients.size()
                << " TCP clients: " << server.tcpClients.size() << "\n";
    }
  }

  addrs->shutdownThreads();

  return EXIT_SUCCESS;
}
