#include "ControlSocket.h"
#include "InputDevice.h"
#include "Message.h"
#include "SharedBuffer.h"
#include "Versions/Version.h"

#include <unistdpp/file.h>
#include <unistdpp/poll.h>
#include <unistdpp/socket.h>
#include <unistdpp/unistdpp.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <unistd.h>

#include <systemd/sd-daemon.h>

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

struct Sockets {
  std::optional<ControlSocket> controlSock = std::nullopt;
  std::optional<FD> tcpSock = std::nullopt;
};

Sockets
getSystemdSockets() {
  auto n = sd_listen_fds(1);
  if (n < 0) {
    std::cerr << "Error getting systemd sockets: " << strerror(-n) << "\n";
    return {};
  }
  if (n == 0) {
    return {};
  }
  std::cout << "Got " << n << " Sockets from systemd\n";

  Sockets result;
  for (int fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + n; fd++) {

    // Check if it's the unix control socket.
    auto res = sd_is_socket(fd, AF_UNIX, SOCK_DGRAM, /* listening */ -1);
    if (res < 0) {
      std::cerr << "Error getting systemd socket: " << strerror(-res) << "\n";
      continue;
    }
    if (res == 1) {
      std::cout << "Got control socket from systemd\n";
      result.controlSock = ControlSocket{ FD{ fd } };
      continue;
    }

    // Check if it's the TCP debug socket.
    res = sd_is_socket(fd, AF_UNSPEC, SOCK_STREAM, /* listening */ 1);
    if (res < 0) {
      std::cerr << "Error getting systemd socket: " << strerror(-res) << "\n";
      continue;
    }
    if (res == 1) {
      std::cout << "Got TCP socket from systemd\n";
      result.tcpSock = FD{ fd };
      continue;
    }
  }

  return result;
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
           static_cast<uint16_t*>(fb.mem.get()) + fbRow * fb_width + params.x1,
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

template<typename Fn>
void
readControlMessage(ControlSocket& serverSock, Fn&& fn) {
  serverSock.recvfrom<UpdateParams>()
    .and_then(std::forward<Fn>(fn))
    .or_else([](auto err) {
      std::cerr << "Recvfrom fail: " << to_string(err) << "\n";
    });
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

} // namespace

int
serverMain(char* argv0, const AddressInfoBase* addrs) { // NOLINT
  setupExitHandler();
  setProcessName(argv0);

  const bool debugMode = checkDebugMode();
  const char* socketAddr = default_sock_addr.data();
  const bool inQemu = !unistdpp::open("/dev/fb0", O_RDONLY).has_value();
  auto systemdSockets = getSystemdSockets();

  // Make uinput devices
  auto devices = makeAllDevices();

  auto serverSock = [&] {
    if (systemdSockets.controlSock.has_value()) {
      std::cout << "Using control socket from systemd\n";
      return std::move(*systemdSockets.controlSock);
    }
    // Setup server socket.
    if (unlink(socketAddr) != 0) {
      perror("Socket unlink");
    }
    ControlSocket serverSock;
    if (!serverSock.init(socketAddr)) {
      perror("Failed to create server socket");
      std::exit(EXIT_FAILURE);
    }
    return serverSock;
  }();

  auto tcpFd = [&]() -> Result<FD> {
    if (systemdSockets.tcpSock.has_value()) {
      std::cout << "Using TCP socket from systemd\n";
      return std::move(*systemdSockets.tcpSock);
    }
    return getTcpSocket(tcp_port);
  }();
  if (!tcpFd) {
    std::cerr << "Unable to start TCP listener: " << to_string(tcpFd.error())
              << "\n";
  }

  std::vector<unistdpp::FD> tcpClients;

  // Get addresses
  if (addrs == nullptr) {
    addrs = getAddresses();
  }

  if (addrs == nullptr) {
    std::cerr << "Failed to get addresses\n";
    return EXIT_FAILURE;
  }

  // Check shared memory
  const auto& fb =
    fatalOnError(SharedFB::getInstance(), "Error creating shared FB");

  // Call the get or create Instance function.
  if (!inQemu) {
    std::cerr << "SWTCON calling init\n";

    // NOLINTNEXTLINE
    auto copyBuffer = std::make_unique<uint16_t[]>(fb_width * fb_height);

    // The init threads does a memset to 0xff. But if we're activated by a
    // systemd socket the shared memory already has some content. So make a
    // backup and preserve it.
    memcpy(copyBuffer.get(), fb.mem.get(), fb_size);
    addrs->initThreads();
    memcpy(fb.mem.get(), copyBuffer.get(), fb_size);

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
      std::cerr << "Poll error: " << to_string(res.error()) << "\n";
      break;
    }

    // Check control socket.
    if (canRead(pollfds.front())) {
      readControlMessage(serverSock, [&](auto msgAndAddr) {
        auto [msg, addr] = msgAndAddr;

        // Emtpy message, just to check init.
        if (msg.x1 == msg.x2 && msg.y1 == msg.y2) {
          std::cerr << "Got init check!\n";
          return serverSock.sendto(true, addr);
        }

        bool res = false;
        if (!inQemu) {
          res = addrs->doUpdate(msg);
        }
        for (auto& client : tcpClients) {
          doTCPUpdate(client, fb, msg);
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
          std::cerr << "Client accept errror: " << to_string(err) << "\n";
        });
    }

    for (size_t idx = fixedFdNum; idx < pollfds.size(); idx++) {
      if (!canRead(pollfds[idx])) {
        continue;
      }

      auto& clientSock = tcpClients[idx - fixedFdNum];
      recvMessage<ClientMsg>(clientSock)
        .map([&](const auto& msg) {
          std::visit([&](auto msg) { handleMsg(fb, clientSock, devices, msg); },
                     msg);
        })
        .or_else([&](auto err) {
          std::cerr << "Reading input: " << to_string(err) << "\n";
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
