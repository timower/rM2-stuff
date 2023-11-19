#include "ControlSocket.h"
#include "ImageHook.h"
#include "InputDevice.h"
#include "Message.h"
#include "Versions/Version.h"

#include <unistdpp/file.h>
#include <unistdpp/poll.h>
#include <unistdpp/socket.h>
#include <unistdpp/unistdpp.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <unistd.h>

using namespace unistdpp;

namespace {
constexpr auto tcp_port = 8888;

std::atomic_bool running = true; // NOLINT

void
onSigint(int num) {
  running = false;
}

void
setupExitHandler() {
  struct sigaction action {};

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

// Starts a tcp server socket
Result<FD>
getTcpSocket(int port) {
  auto listenfd = TRY(unistdpp::socket(AF_INET, SOCK_STREAM, 0));

  // lose the pesky "Address already in use" error message
  int yes = 1;
  TRY(unistdpp::setsockopt(
    listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

  TRY(unistdpp::bind(listenfd, Address::fromHostPort(INADDR_ANY, port)));
  TRY(unistdpp::listen(listenfd, 1));

  // return unistdpp::accept(listenfd, nullptr, nullptr);
  return listenfd;
}

bool
doTCPUpdate(unistdpp::FD& fd, const UpdateParams& params) {
  if (auto res = fd.writeAll(params); !res) {
    std::cerr << "Error writing: " << toString(res.error()) << "\n";
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
           fb.mem + fbRow * fb_width + params.x1, // NOLINT
           width * sizeof(uint16_t));
  }

  const auto writeSize = size * sizeof(uint16_t);
  auto res = fd.writeAll(buffer.data(), writeSize);
  if (!res) {
    std::cerr << "Error writing: " << toString(res.error()) << "\n";
    fd.close();
    return false;
  }

  return true;
}

template<typename Fn>
void
readControlMessage(ControlSocket& serverSock, Fn&& fn) {
  serverSock.recvfrom<UpdateParams>()
    .and_then(std::forward<Fn>(fn))
    .or_else([](auto err) {
      std::cerr << "Recvfrom fail: " << unistdpp::toString(err) << "\n";
    });
}

int
serverMain(int argc, char* argv[], char** envp) { // NOLINT
  setupExitHandler();
  setProcessName(argv[0]); // NOLINT

  const bool debugMode = checkDebugMode();
  const char* socketAddr = default_sock_addr.data();
  const bool inQemu = !unistdpp::open("/dev/fb0", O_RDONLY).has_value();

  // Make uinput devices
  AllUinputDevices devices;
  if (inQemu) {
    devices = makeAllDevices();
  } else {
    // Only make the wacom device, used for faking input from tcp clients.
    devices.wacom = makeWacomDevice();
  }

  // Setup server socket.
  if (unlink(socketAddr) != 0) {
    perror("Socket unlink");
  }

  ControlSocket serverSock;
  if (!serverSock.init(socketAddr)) {
    return EXIT_FAILURE;
  }

  auto tcpFd = getTcpSocket(tcp_port);
  if (!tcpFd) {
    std::cerr << "Unable to start TCP listener: " << toString(tcpFd.error())
              << "\n";
  }
  std::vector<unistdpp::FD> tcpClients;

  // Get addresses
  const auto* addrs = getAddresses();
  if (addrs == nullptr) {
    std::cerr << "Failed to get addresses\n";
    return EXIT_FAILURE;
  }

  // Open shared memory
  if (fb.mem == nullptr) {
    return EXIT_FAILURE;
  }
  std::cerr << "Setting mem\n";
  memset(fb.mem, UINT8_MAX, fb_size);

  // Call the get or create Instance function.
  if (!inQemu) {
    std::cerr << "SWTCON calling init\n";
    addrs->initThreads();
    std::cout << "SWTCON initalized!\n";
  } else {
    std::cout << "In QEMU, not starting SWTCON\n";
  }

  const auto fixedFdNum = 1 + (tcpFd.has_value() ? 1 : 0);
  std::vector<pollfd> pollfds;

  std::cout << "rm2fb-server started!\n";
  while (running) {
    pollfds.clear();
    pollfds.reserve(tcpClients.size() + fixedFdNum);
    pollfds.emplace_back(waitFor(serverSock.sock, Wait::Read));
    if (tcpFd) {
      pollfds.emplace_back(waitFor(*tcpFd, Wait::Read));
    }

    std::transform(
      tcpClients.begin(),
      tcpClients.end(),
      std::back_inserter(pollfds),
      [](const auto& client) { return waitFor(client, Wait::Read); });

    if (auto res = unistdpp::poll(pollfds); !res) {
      std::cerr << "Poll error: " << toString(res.error()) << "\n";
      break;
    }

    // Check control socket.
    if (canRead(pollfds.front())) {
      readControlMessage(serverSock, [&](auto msgAndAddr) {
        auto [msg, addr] = msgAndAddr;

        bool res = false;
        if (!inQemu) {
          res = addrs->doUpdate(msg);
        }
        for (auto& client : tcpClients) {
          doTCPUpdate(client, msg);
        }

        // Don't log Stroke updates, unless debug mode is on.
        if (debugMode || msg.flags != 4) {
          std::cerr << "UPDATE " << msg << ": " << res << "\n";
        }

        return serverSock.sendto(res, addr);
      });
    }

    // If we don't have any tcp clients, there are not other FDs to check.
    if (!tcpFd) {
      continue;
    }

    if (canRead(pollfds[1])) {
      std::cout << "Accepting new client!\n";

      unistdpp::accept(*tcpFd, nullptr, nullptr)
        .transform(
          [&](auto client) { tcpClients.emplace_back(std::move(client)); })
        .or_else([](auto err) {
          std::cerr << "Client accept errror: " << toString(err) << "\n";
        });
    }

    for (size_t idx = fixedFdNum; idx < pollfds.size(); idx++) {
      if (!canRead(pollfds[idx])) {
        continue;
      }

      auto& clientSock = tcpClients[idx - fixedFdNum];
      recvMessage<ClientMsg>(clientSock)
        .map([&](const auto& msg) {
          if (std::holds_alternative<GetUpdate>(msg)) {
            doTCPUpdate(tcpClients.back(),
                        { .y1 = 0,
                          .x1 = 0,
                          .y2 = fb_height - 1,
                          .x2 = fb_width - 1,
                          .flags = 0,
                          .waveform = 0 });
            return;
          }
          if (devices.wacom) {
            sendInput(std::get<Input>(msg), *devices.wacom);
          }
        })
        .or_else([&](auto err) {
          std::cerr << "Reading input: " << toString(err) << "\n";
          if (err == unistdpp::FD::eof_error) {
            clientSock.close();
          }
        });
    }

    // Remove closed clients
    tcpClients.erase(
      std::remove_if(tcpClients.begin(),
                     tcpClients.end(),
                     [](const auto& sock) { return !sock.isValid(); }),
      tcpClients.end());
  }

  return EXIT_SUCCESS;
}

} // namespace

extern "C" {

int
__libc_start_main(int (*_main)(int, char**, char**), // NOLINT
                  int argc,
                  char** argv,
                  int (*init)(int, char**, char**),
                  void (*fini)(),
                  void (*rtldFini)(),
                  void* stackEnd) {

  std::cout << "STARTING RM2FB\n";

  // NOLINTNEXTLINE
  auto* funcMain = reinterpret_cast<decltype(&__libc_start_main)>(
    dlsym(RTLD_NEXT, "__libc_start_main"));

  return funcMain(serverMain, argc, argv, init, fini, rtldFini, stackEnd);
};
}
