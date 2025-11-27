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
#include <sys/stat.h>
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
  std::cerr << "Got " << n << " Sockets from systemd\n";

  Sockets result;
  for (int fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + n; fd++) {

    // Check if it's the unix control socket.
    auto res = sd_is_socket(fd, AF_UNIX, SOCK_STREAM, /* listening */ -1);
    if (res < 0) {
      std::cerr << "Error getting systemd socket: " << strerror(-res) << "\n";
      continue;
    }
    if (res == 1) {
      std::cerr << "Got control socket from systemd\n";
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
      std::cerr << "Got TCP socket from systemd\n";
      result.tcpSock = FD{ fd };
      continue;
    }
  }

  return result;
}

// Starts a tcp server socket
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

struct Server {
  const bool debugMode = checkDebugMode();
  const bool inQemu = !unistdpp::open("/dev/fb0", O_RDONLY).has_value();

  AllUinputDevices uinputDevices;

  Sockets systemdSocket;

  ControlSocket serverSock;
  unistdpp::FD tcpListenSock;

  std::vector<unistdpp::FD> unixClients;
  std::vector<unistdpp::FD> tcpClients;

  const SharedFB& fb;

  const AddressInfoBase* hookAddrs;

  std::vector<pollfd> pollfds;

  static ControlSocket getServerSock(Sockets& systemdSockets) {
    if (systemdSockets.controlSock.has_value()) {
      std::cerr << "Using control socket from systemd\n";
      return std::move(*systemdSockets.controlSock);
    }

    // Setup server socket.
    const char* socketAddr = default_sock_addr.data();
    if (unlink(socketAddr) != 0) {
      perror("Socket unlink");
    }

    ControlSocket serverSock;
    if (!serverSock.init(socketAddr)) {
      perror("Failed to create server socket");
      std::exit(EXIT_FAILURE);
    }

    if (!serverSock.listen(1)) {
      perror("Failed to create server socket");
      std::exit(EXIT_FAILURE);
    }

    return serverSock;
  }

  static unistdpp::FD getTcpSocket(Sockets& systemdSockets) {
    if (systemdSockets.tcpSock.has_value()) {
      std::cerr << "Using TCP socket from systemd\n";
      return std::move(*systemdSockets.tcpSock);
    }
    return createTCPSocket(tcp_port)
      .or_else([](auto err) {
        std::cerr << "Error creating TCP socket: " << to_string(err) << "\n";
      })
      .value_or(FD());
  }

  Server(const AddressInfoBase* addrs)
    : uinputDevices(makeAllDevices())
    , systemdSocket(getSystemdSockets())
    , serverSock(getServerSock(systemdSocket))
    , tcpListenSock(getTcpSocket(systemdSocket))
    , fb(fatalOnError(SharedFB::getInstance())) {
    // Get addresses
    if (addrs == nullptr) {
      hookAddrs = getAddresses();
    } else {
      hookAddrs = addrs;
    }

    if (hookAddrs == nullptr) {
      std::cerr << "Failed to get addresses\n";
      std::exit(EXIT_FAILURE);
    }
  }

  void initSWTCON() {
    // Call the get or create Instance function.
    if (!inQemu) {
      std::cerr << "SWTCON calling init\n";

      // NOLINTNEXTLINE
      auto copyBuffer = std::make_unique<uint16_t[]>(fb_width * fb_height);

      // The init threads does a memset to 0xff. But if we're activated by a
      // systemd socket the shared memory already has some content. So make a
      // backup and preserve it.
      memcpy(copyBuffer.get(), fb.mem.get(), fb_size);
      hookAddrs->initThreads();
      memcpy(fb.mem.get(), copyBuffer.get(), fb_size);

      std::cerr << "SWTCON initalized!\n";
    } else {
      std::cerr << "In QEMU, not starting SWTCON\n";
    }
  }

  std::size_t numListenFds() const {
    return 1 + (tcpListenSock.isValid() ? 1 : 0);
  }

  std::vector<pollfd>& getPollFDs() {
    pollfds.clear();
    pollfds.reserve(numListenFds() + unixClients.size() + tcpClients.size());

    pollfds.emplace_back(waitFor(serverSock.sock, Wait::Read));
    if (tcpListenSock.isValid()) {
      pollfds.emplace_back(waitFor(tcpListenSock, Wait::Read));
    }

    std::transform(
      unixClients.begin(),
      unixClients.end(),
      std::back_inserter(pollfds),
      [](const auto& client) { return waitFor(client, Wait::Read); });

    std::transform(
      tcpClients.begin(),
      tcpClients.end(),
      std::back_inserter(pollfds),
      [](const auto& client) { return waitFor(client, Wait::Read); });

    return pollfds;
  }

  bool dropClients() {

    // Remove closed clients
    auto prevUnixClients = unixClients.size();
    unixClients.erase(
      std::remove_if(unixClients.begin(),
                     unixClients.end(),
                     [&](const auto& sock) { return !sock.isValid(); }),
      unixClients.end());

    auto prevTcpClients = tcpClients.size();
    tcpClients.erase(
      std::remove_if(tcpClients.begin(),
                     tcpClients.end(),
                     [&](const auto& sock) { return !sock.isValid(); }),
      tcpClients.end());

    return unixClients.size() != prevUnixClients ||
           tcpClients.size() != prevTcpClients;
  }

  bool acceptUnixClient() {
    auto sock = serverSock.accept();
    if (!sock) {
      std::cerr << "Unix client accept error: " << to_string(sock.error())
                << "\n";
      return false;
    }

    std::cerr << "New unix client!\n";
    unixClients.emplace_back(std::move(*sock));
    return true;
  }

  bool acceptTcpClient() {
    auto sock = unistdpp::accept(tcpListenSock, nullptr, nullptr);
    if (!sock) {
      std::cerr << "Client accept errror: " << to_string(sock.error()) << "\n";
      return false;
    }

    std::cerr << "New tcp client!\n";
    tcpClients.emplace_back(std::move(*sock));
    return true;
  }

  void readUnixSock(unistdpp::FD& sock) {
    sock.readAll<UpdateParams>()
      .and_then([&](auto msg) {
        // Emtpy message, just to check init.
        if (msg.x1 == msg.x2 && msg.y1 == msg.y2) {
          std::cerr << "Got init check!\n";
          return sock.writeAll(true);
        }

        bool res = false;
        if (!inQemu) {
          res = hookAddrs->doUpdate(msg);
        }
        for (auto& client : tcpClients) {
          doTCPUpdate(client, fb, msg);
        }

        // Don't log Stroke updates, unless debug mode is on.
        if (debugMode) {
          std::cerr << "UPDATE " << msg << ": " << res << "\n";
        }

        return sock.writeAll(res);
      })
      .or_else([&](auto err) {
        std::cerr << "Unix client fail: " << to_string(err) << "\n";
        if (err == unistdpp::FD::eof_error || err == std::errc::broken_pipe) {
          sock.close();
        }
      });
  }

  void readTCPSock(unistdpp::FD& sock) {
    recvMessage<ClientMsg>(sock)
      .transform([&](const auto& msg) {
        std::visit([&](auto msg) { handleMsg(fb, sock, uinputDevices, msg); },
                   msg);
      })
      .or_else([&](auto err) {
        std::cerr << "Reading input: " << to_string(err) << "\n";
        if (err == unistdpp::FD::eof_error) {
          sock.close();
        }
      });
  }

  bool poll() {
    bool clientChanges = false;

    const auto nListenFds = numListenFds();
    const auto numUnixClients = unixClients.size();
    const auto numTcpClients = tcpClients.size();

    if (auto res = unistdpp::poll(getPollFDs()); !res) {
      std::cerr << "Poll error: " << to_string(res.error()) << "\n";
      return false;
    }

    // Check control socket.
    if (canRead(pollfds.front())) {
      clientChanges |= acceptUnixClient();
    }

    for (size_t i = 0; i < numUnixClients; i++) {
      if (canRead(pollfds[nListenFds + i])) {
        readUnixSock(unixClients[i]);
      }
    }

    // If we don't have any tcp clients, there are not other FDs to check.
    if (tcpListenSock.isValid()) {
      if (canRead(pollfds[1])) {
        clientChanges |= acceptTcpClient();
      }

      for (size_t i = 0; i < numTcpClients; i++) {
        if (canRead(pollfds[nListenFds + numUnixClients + i])) {
          readTCPSock(tcpClients[i]);
        }
      }
    }

    clientChanges |= dropClients();
    return clientChanges;
  }
};

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

  return EXIT_SUCCESS;
}
