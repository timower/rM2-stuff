#include "rm2fb/ControlSocket.h"
#include "rm2fb/Message.h"
#include "rm2fb/SharedBuffer.h"

#include "InputDevice.h"
#include "InputMonitor.h"
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
template<class T, class Alloc, class Pred>

constexpr typename std::vector<T, Alloc>::size_type
erase_if(std::vector<T, Alloc>& c, Pred pred) {
  auto it = std::remove_if(c.begin(), c.end(), pred);
  auto r = c.end() - it;
  c.erase(it, c.end());
  return r;
}
constexpr auto tcp_port = 8888;

std::atomic_bool running = true; // NOLINT

void
onSigint(int num) {
  running = false;
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

struct UnixClient {
  unistdpp::FD fd;
  pid_t pid;

  std::unique_ptr<std::array<uint8_t, fb_size>> savedFB;
  bool dontPause;
};

struct Server : ControlInterface {
  const bool debugMode = checkDebugMode();
  const bool inQemu = !unistdpp::open("/dev/fb0", O_RDONLY).has_value();

  AllUinputDevices uinputDevices;
  InputMonitor inputMonitor;

  unistdpp::FD serverSock;
  unistdpp::FD tcpListenSock;

  ControlServer controlServer;

  pid_t frontPID = 0;
  std::vector<UnixClient> unixClients;
  std::vector<unistdpp::FD> tcpClients;

  const SharedFB& fb;

  const AddressInfoBase* hookAddrs;

  std::vector<pollfd> pollfds;

  void getSystemdSockets() {
    auto n = sd_listen_fds(1);
    if (n < 0) {
      std::cerr << "Error getting systemd sockets: " << strerror(-n) << "\n";
      return;
    }
    if (n == 0) {
      return;
    }
    std::cerr << "Got " << n << " Sockets from systemd\n";

    for (int fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + n; fd++) {

      // Check if it's the unix server socket.
      auto res = sd_is_socket(fd, AF_UNIX, SOCK_STREAM, /* listening */ 1);
      if (res < 0) {
        std::cerr << "Error getting systemd socket: " << strerror(-res) << "\n";
        continue;
      }
      if (res == 1) {
        std::cerr << "Got server socket from systemd\n";
        serverSock = FD{ fd };
        continue;
      }

      // Check if it's the unix control socket.
      res = sd_is_socket(fd, AF_UNIX, SOCK_DGRAM, /* listening */ -1);
      if (res < 0) {
        std::cerr << "Error getting systemd socket: " << strerror(-res) << "\n";
        continue;
      }
      if (res == 1) {
        std::cerr << "Got control socket from systemd\n";
        controlServer.sock = FD{ fd };
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
        tcpListenSock = FD{ fd };
        continue;
      }
    }
  }

  void initServerSocket() {
    if (serverSock.isValid()) {
      std::cerr << "Using server socket from systemd\n";
      return;
    }

    // Setup server socket.
    const char* socketAddr = default_sock_addr.data();
    if (unlink(socketAddr) != 0) {
      perror("Socket unlink");
    }

    serverSock = fatalOnError(unistdpp::socket(AF_UNIX, SOCK_STREAM, 0),
                              "Failed to create server socket: ");
    fatalOnError(unistdpp::bind(serverSock, Address::fromUnixPath(socketAddr)),
                 "Failed to bind server sock");
    fatalOnError(unistdpp::listen(serverSock, 5),
                 "Failed to listen on server sock");
  }

  void initTcpSocket() {
    if (tcpListenSock.isValid()) {
      std::cerr << "Using TCP socket from systemd\n";
      return;
    }
    tcpListenSock =
      createTCPSocket(tcp_port)
        .or_else([](auto err) {
          std::cerr << "Error creating TCP socket: " << to_string(err) << "\n";
        })
        .value_or(FD());
  }

  Server(const AddressInfoBase* addrs)
    : controlServer(*this), fb(fatalOnError(SharedFB::getInstance())) {

    getSystemdSockets();
    initServerSocket();
    controlServer.maybeInit();
    initTcpSocket();

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

    inputMonitor.openDevices();
    uinputDevices = makeAllDevices();
    inputMonitor.startMonitor();
  }

  void initSWTCON() {
    // Call the get or create Instance function.
    if (!inQemu) {
      std::cerr << "SWTCON calling init\n";

      auto copyBuffer = std::make_unique<std::array<uint8_t, fb_size>>();

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

  bool doUpdate(const UpdateParams& msg) {
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
    return res;
  }

  bool dropClients() {

    // Remove closed clients
    auto removedUnixClients = erase_if(
      unixClients, [&](const auto& client) { return !client.fd.isValid(); });

    auto removedTcpClients =
      erase_if(tcpClients, [&](const auto& sock) { return !sock.isValid(); });

    return removedUnixClients != 0 || removedTcpClients != 0;
  }

  void resume(UnixClient& client) {
    frontPID = client.pid;
    std::cerr << "Resuming: " << frontPID << "\n";

    if (client.dontPause) {
      return;
    }

    if (client.savedFB) {
      memcpy(fb.mem.get(), client.savedFB.get(), fb_size);
      doUpdate(UpdateParams{
        .y1 = 0,
        .x1 = 0,
        .y2 = fb_height - 1,
        .x2 = fb_width - 1,
        .flags = 0,
        .waveform = WAVEFORM_MODE_GL16 | UpdateParams::ioctl_waveform_flag,
        .temperatureOverride = 0,
        .extraMode = 0,
      });
    }

    inputMonitor.flood();
    kill(-frontPID, SIGCONT);
  }

  auto findClient(pid_t pid) {
    return std::find_if(
      unixClients.begin(), unixClients.end(), [pid](const auto& client) {
        return client.pid == pid;
      });
  }

  bool pause(pid_t pid) {
    auto it = findClient(pid);
    if (it == unixClients.end()) {
      std::cerr << "No client found with paused pid\n";
      return false;
    }

    if (it->dontPause) {
      return true;
    }

    std::cerr << "pausing: " << pid << "\n";
    kill(-pid, SIGSTOP);

    if (it->savedFB == nullptr) {
      it->savedFB = std::make_unique<std::array<uint8_t, fb_size>>();
    }
    memcpy(it->savedFB.get(), fb.mem.get(), fb_size);

    return true;
  }

  unistdpp::Result<std::vector<Client>> getClients() override {
    std::vector<Client> clients;
    std::transform(unixClients.begin(),
                   unixClients.end(),
                   std::back_inserter(clients),
                   [this](const auto& client) {
                     return Client{
                       .pid = client.pid,
                       .active = client.pid == frontPID,
                     };
                   });
    return clients;
  }

  unistdpp::Result<unistdpp::FD> getFramebuffer(pid_t pid) override {
    // TODO: impl
    return {};
  }

  unistdpp::Result<void> switchTo(pid_t pid) override {
    if (pid == frontPID) {
      return {};
    }

    auto it = findClient(pid);
    if (it == unixClients.end()) {
      return tl::unexpected(std::errc::bad_file_descriptor);
    }

    if (frontPID != 0) {
      pause(frontPID);
    }
    resume(*it);
    return {};
  }

  unistdpp::Result<void> setLauncher(pid_t pid) override {
    auto it = findClient(pid);
    if (it == unixClients.end()) {
      return tl::unexpected(std::errc::bad_file_descriptor);
    }

    it->dontPause = true;
    return {};
  }

  void updateFrontClient() {
    if (std::any_of(unixClients.begin(),
                    unixClients.end(),
                    [this](auto& client) { return client.pid == frontPID; })) {
      return;
    }

    if (unixClients.empty()) {
      frontPID = 0;
      return;
    }

    std::cerr << "Front client gone ";
    resume(unixClients.back());
  }

  bool acceptUnixClient() {
    auto sock = unistdpp::accept(serverSock, nullptr, nullptr);
    if (!sock) {
      std::cerr << "Unix client accept error: " << to_string(sock.error())
                << "\n";
      return false;
    }

    ucred peerCred;
    socklen_t len = sizeof(peerCred);
    if (auto err =
          unistdpp::getsockopt(*sock, SOL_SOCKET, SO_PEERCRED, &peerCred, &len);
        !err) {
      std::cerr << "Error getting peercred: " << to_string(err.error()) << "\n";
      return false;
    }

    std::cerr << "New unix client: " << std::dec << peerCred.pid << "!\n";
    unixClients.emplace_back(UnixClient{
      .fd = std::move(*sock),
      .pid = peerCred.pid,
      .savedFB = nullptr,
      .dontPause = false,
    });

    if (frontPID != 0 && peerCred.pid != frontPID) {
      pause(frontPID);
    }

    frontPID = unixClients.back().pid;

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

  void readUnixSock(UnixClient& client) {
    client.fd.readAll<UpdateParams>()
      .and_then([&](auto msg) {
        // Emtpy message, just to check init.
        if (msg.x1 == msg.x2 && msg.y1 == msg.y2) {
          std::cerr << "Got init check!\n";
          return client.fd.writeAll(true);
        }

        bool res = doUpdate(msg);
        return client.fd.writeAll(res);
      })
      .or_else([&](auto err) {
        std::cerr << "Unix client fail: " << to_string(err) << "\n";
        if (err == unistdpp::FD::eof_error || err == std::errc::broken_pipe) {
          client.fd.close();
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

  // void readControlMsg() {
  //   recvMessageFrom<ControlMsg>(controlSock)
  //     .transform([&](const auto& pair) {
  //       std::visit([&](const auto& msg) { controlMsg(msg); }, pair.second);
  //     })
  //     .or_else([](auto err) {
  //     });
  // }

  int getPollFDs() {
    pollfds.clear();
    pollfds.reserve(4 + unixClients.size() + tcpClients.size());

    pollfds.emplace_back(waitFor(inputMonitor.udevMonitorFd, Wait::Read));
    pollfds.emplace_back(waitFor(serverSock, Wait::Read));
    pollfds.emplace_back(waitFor(controlServer.sock, Wait::Read));
    if (tcpListenSock.isValid()) {
      pollfds.emplace_back(waitFor(tcpListenSock, Wait::Read));
    }

    int res = pollfds.size();

    std::transform(
      unixClients.begin(),
      unixClients.end(),
      std::back_inserter(pollfds),
      [](const auto& client) { return waitFor(client.fd, Wait::Read); });

    std::transform(
      tcpClients.begin(),
      tcpClients.end(),
      std::back_inserter(pollfds),
      [](const auto& client) { return waitFor(client, Wait::Read); });

    return res;
  }

  bool poll() {
    bool clientChanges = false;

    const auto nListenFds = getPollFDs();
    const auto numUnixClients = unixClients.size();
    const auto numTcpClients = tcpClients.size();

    if (auto res = unistdpp::poll(pollfds); !res) {
      std::cerr << "Poll error: " << to_string(res.error()) << "\n";
      return false;
    }

    // Check server socket.
    if (canRead(pollfds[0])) {
      inputMonitor.handleNewDevices();
    }

    if (canRead(pollfds[1])) {
      clientChanges |= acceptUnixClient();
    }

    if (canRead(pollfds[2])) {
      controlServer.handleMsg().or_else([](auto err) {
        std::cerr << "Control error: " << to_string(err) << "\n";
      });
    }

    for (size_t i = 0; i < numUnixClients; i++) {
      if (isClosed(pollfds[nListenFds + i])) {
        std::cerr << "Detected HUP\n";
        unixClients[i].fd.close();
        continue;
      }

      if (canRead(pollfds[nListenFds + i])) {
        readUnixSock(unixClients[i]);
      }
    }

    // If we don't have any tcp clients, there are not other FDs to check.
    if (tcpListenSock.isValid()) {
      if (canRead(pollfds[3])) {
        clientChanges |= acceptTcpClient();
      }

      for (size_t i = 0; i < numTcpClients; i++) {
        if (isClosed(pollfds[nListenFds + numUnixClients + i])) {
          tcpClients[i].close();
          continue;
        }
        if (canRead(pollfds[nListenFds + numUnixClients + i])) {
          readTCPSock(tcpClients[i]);
        }
      }
    }

    if (dropClients()) {
      clientChanges = true;
      updateFrontClient();
    }
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
