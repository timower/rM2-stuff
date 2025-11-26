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

#include <libevdev/libevdev.h>
#include <libudev.h>

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

constexpr auto touch_flood_size = 8 * 512 * 4;
constexpr auto key_flood_size = 8 * 64;

auto
mkEvent(int a, int b, int v) {
  input_event r{};
  r.type = a;
  r.code = b;
  r.value = v;
  r.input_event_sec = 0;
  r.input_event_usec = 0;
  return r;
}

auto*
getTouchFlood() {
  static const auto* floodBuffer = [] {
    // NOLINTNEXTLINE
    static const auto ret = std::make_unique<input_event[]>(touch_flood_size);
    for (int i = 0; i < touch_flood_size;) {
      ret[i++] = mkEvent(EV_ABS, ABS_DISTANCE, 1);
      ret[i++] = mkEvent(EV_SYN, 0, 0);
      ret[i++] = mkEvent(EV_ABS, ABS_DISTANCE, 2);
      ret[i++] = mkEvent(EV_SYN, 0, 0);
    }
    return ret.get();
  }();
  return floodBuffer;
}

auto*
getKeyFlood() {
  static const auto* floodBuffer = [] {
    // NOLINTNEXTLINE
    static const auto ret = std::make_unique<input_event[]>(key_flood_size);
    for (int i = 0; i < key_flood_size;) {
      ret[i++] = mkEvent(EV_KEY, KEY_LEFTALT, 1);
      ret[i++] = mkEvent(EV_SYN, SYN_REPORT, 0);
      ret[i++] = mkEvent(EV_KEY, KEY_LEFTALT, 0);
      ret[i++] = mkEvent(EV_SYN, SYN_REPORT, 0);
    }
    return ret.get();
  }();
  return floodBuffer;
}

struct Sockets {
  std::optional<FD> controlSock = std::nullopt;
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
      result.controlSock = FD{ fd };
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

struct InputMonitor {
  ~InputMonitor() {
    if (udevMonitor != nullptr) {
      udev_monitor_unref(udevMonitor);
    }
    if (udevHandle != nullptr) {
      udev_unref(udevHandle);
    }
  }

  void openDevices() {
    udevHandle = udev_new();

    udev_enumerate* enumerate = udev_enumerate_new(udevHandle);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry* devList = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry = nullptr;

    udev_list_entry_foreach(entry, devList) {
      const char* path = udev_list_entry_get_name(entry);
      struct udev_device* dev = udev_device_new_from_syspath(udevHandle, path);

      if (dev != nullptr) {
        handleUdev(*dev);
        udev_device_unref(dev);
      }
    }

    udev_enumerate_unref(enumerate);
  }

  void startMonitor() {
    udevMonitor = udev_monitor_new_from_netlink(udevHandle, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(
      udevMonitor, "input", nullptr);
    udev_monitor_enable_receiving(udevMonitor);
    udevMonitorFd = unistdpp::FD(udev_monitor_get_fd(udevMonitor));
  }

  void handleNewDevices() {
    auto* dev = udev_monitor_receive_device(udevMonitor);
    if (dev != nullptr) {
      handleUdev(*dev);
      udev_device_unref(dev);
    }
  }

  void flood() {
    for (const auto& [_, dev] : devices) {
      if (dev.isTouch) {
        (void)dev.fd.writeAll(getTouchFlood(),
                              touch_flood_size * sizeof(input_event));
      } else {
        (void)dev.fd.writeAll(getKeyFlood(),
                              key_flood_size * sizeof(input_event));
      }
    }
  }

  void openDevice(std::string input) {
    if (devices.count(input) != 0) {
      return;
    }

    auto fd = unistdpp::open(input.c_str(), O_RDWR | O_NONBLOCK);
    if (!fd) {
      return;
    }

    libevdev* dev;
    if (libevdev_new_from_fd(fd->fd, &dev) < 0) {
      return;
    }

    if (libevdev_has_event_type(dev, EV_ABS) != 0) {
      // multi-touch screen -> ev_abs & abs_mt_slot
      if (libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT) != 0) {
        std::cerr << "Tracking MT dev: " << input << "\n";
        devices[input] = Device{ std::move(*fd), true };
      }
      // Ignore pen input
    } else if (libevdev_has_event_type(dev, EV_KEY) != 0) {
      std::cerr << "Tracking key dev: " << input << "\n";
      devices[input] = Device{ std::move(*fd), false };
    }

    libevdev_free(dev);
  }

  void handleUdev(udev_device& dev) {
    const auto* devnode = udev_device_get_devnode(&dev);
    if (devnode == nullptr) {
      return;
    }

    const auto* action = udev_device_get_action(&dev);
    if (action == nullptr || action == std::string_view("add")) {
      openDevice(devnode);
      return;
    }

    devices.erase(devnode);
  }

  udev* udevHandle = nullptr;
  udev_monitor* udevMonitor = nullptr;

  unistdpp::FD udevMonitorFd;

  struct Device {
    FD fd;
    bool isTouch;
  };
  std::unordered_map<std::string, Device> devices;
};

struct UnixClient {
  unistdpp::FD fd;
  pid_t pid;

  std::unique_ptr<std::array<uint8_t, fb_size>> savedFB;
};

struct Server {
  const bool debugMode = checkDebugMode();
  const bool inQemu = !unistdpp::open("/dev/fb0", O_RDONLY).has_value();

  AllUinputDevices uinputDevices;
  InputMonitor inputMonitor;

  Sockets systemdSocket;

  unistdpp::FD serverSock;
  unistdpp::FD tcpListenSock;

  pid_t frontPID = 0;
  std::vector<UnixClient> unixClients;
  std::vector<unistdpp::FD> tcpClients;

  const SharedFB& fb;

  const AddressInfoBase* hookAddrs;

  std::vector<pollfd> pollfds;

  static FD getServerSock(Sockets& systemdSockets) {
    if (systemdSockets.controlSock.has_value()) {
      std::cerr << "Using control socket from systemd\n";
      return std::move(*systemdSockets.controlSock);
    }

    // Setup server socket.
    const char* socketAddr = default_sock_addr.data();
    if (unlink(socketAddr) != 0) {
      perror("Socket unlink");
    }

    auto sock = fatalOnError(unistdpp::socket(AF_UNIX, SOCK_STREAM, 0),
                             "Failed to create server socket: ");
    fatalOnError(unistdpp::bind(sock, Address::fromUnixPath(socketAddr)),
                 "Failed to bind server sock");
    fatalOnError(unistdpp::listen(sock, 5), "Failed to listen on server sock");

    return sock;
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
    : systemdSocket(getSystemdSockets())
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

  std::size_t numListenFds() const {
    return 1 + (tcpListenSock.isValid() ? 1 : 0);
  }

  std::vector<pollfd>& getPollFDs() {
    pollfds.clear();
    pollfds.reserve(numListenFds() + unixClients.size() + tcpClients.size());

    pollfds.emplace_back(waitFor(serverSock, Wait::Read));
    if (tcpListenSock.isValid()) {
      pollfds.emplace_back(waitFor(tcpListenSock, Wait::Read));
    }

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

    pollfds.push_back(waitFor(inputMonitor.udevMonitorFd, Wait::Read));

    return pollfds;
  }

  bool dropClients() {

    // Remove closed clients
    auto prevUnixClients = unixClients.size();
    unixClients.erase(
      std::remove_if(unixClients.begin(),
                     unixClients.end(),
                     [&](const auto& client) { return !client.fd.isValid(); }),
      unixClients.end());

    auto removedTcpClients =
      erase_if(tcpClients, [&](const auto& sock) { return !sock.isValid(); });

    return unixClients.size() != prevUnixClients || removedTcpClients != 0;
  }

  void resume(UnixClient& client) {
    frontPID = client.pid;
    std::cerr << "Front client exited, resuming: " << frontPID << "\n";

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

  void pause(pid_t pid) {
    std::cerr << "pausing: " << frontPID << "\n";
    kill(-frontPID, SIGSTOP);

    auto it =
      std::find_if(unixClients.begin(),
                   unixClients.end(),
                   [pid](const auto& client) { return client.pid == pid; });
    if (it == unixClients.end()) {
      std::cerr << "No client found with paused pid\n";
      return;
    }

    if (it->savedFB == nullptr) {
      it->savedFB = std::make_unique<std::array<uint8_t, fb_size>>();
    }
    memcpy(it->savedFB.get(), fb.mem.get(), fb_size);
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
    unixClients.emplace_back(
      UnixClient{ std::move(*sock), peerCred.pid, nullptr });

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

    if (canRead(pollfds.back())) {
      inputMonitor.handleNewDevices();
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
      if (canRead(pollfds[1])) {
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
