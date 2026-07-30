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
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <sys/file.h>
#include <sys/stat.h>
#include <type_traits>
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

// Guards against two rm2fb-server processes running concurrently.
// Deliberately a separate lock file from swtcon's own /tmp/epd.lock
// (see ServerSwtcon.cpp's skipPidLock=true) - that lock is reserved for
// keeping this server's swtcon instance from fighting xochitl's own,
// independent one over the same hardware, coordinated instead via
// SIGSTOP/SIGCONT (suspendForXochitl/resumeForXochitl); it says nothing
// about whether a second rm2fb-server itself is already running, which
// skipPidLock=true no longer guards against on its own.
FD
acquireServerLock() {
  // unistdpp::open only wraps the 2-arg (path, flags) form of ::open, so
  // O_CREAT's mode argument needs the raw POSIX call here, same as
  // swtcon's own create_pid_file() (libs/swtcon/init.cpp).
  FD fd{ ::open("/tmp/rm2fb-server.lock", O_RDWR | O_CREAT, 0666) };
  if (!fd.isValid()) {
    perror("Failed to open /tmp/rm2fb-server.lock");
    std::exit(EXIT_FAILURE);
  }
  if (flock(fd.fd, LOCK_EX | LOCK_NB) != 0) {
    std::cerr << "Another rm2fb-server instance is already running\n";
    std::exit(EXIT_FAILURE);
  }
  return fd;
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
  // filename component, comparing equal to nothing callers actually check
  // against (see acceptUnixClient's isXochitl - this silently made it
  // false for real xochitl, since it's always launched with args).
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

struct UnixClient {
  unistdpp::FD sock;
  pid_t pid;
  bool dontPause;

  // Whether this client runs its own, independent swtcon instance
  // (currently only xochitl, via ClientSwtcon.cpp/rm2fb_client_swtcon)
  // that needs SIGSTOP/SIGCONT + AddressInfoBase::suspendForXochitl()/
  // resumeForXochitl() coordination instead of being driven through
  // doUpdate(). Detected once at accept time via /proc/<pid>/cmdline.
  bool isXochitl;
  // Last IdleUpdate value reported over this client's control socket.
  // Only meaningful when isXochitl.
  bool idle = false;
  // Set if this client's Init message arrived while it wasn't front yet -
  // see requestSwitch()'s deferred-switch case (a fresh client connecting
  // while the current front is a busy xochitl client doesn't get a
  // buffer/frontPID/Init reply at all until that switch actually
  // resolves). resume() replies to it (and clears it) once the client
  // actually becomes front. For the common, non-deferred case this is
  // always false by the time Init arrives, since resume() already runs
  // synchronously within acceptUnixClient() well before the new client's
  // Init message could have reached the server.
  bool initPending = false;

  unistdpp::FD memFD;
};

struct Server : ControlInterface {
  const bool debugMode = checkDebugMode();
  const bool inQemu = !unistdpp::open("/dev/fb0", O_RDONLY).has_value();

  // Held for the process's lifetime once acquired in the constructor -
  // see acquireServerLock().
  unistdpp::FD serverLockFd = acquireServerLock();

  AllUinputDevices uinputDevices;
  InputMonitor inputMonitor;

  unistdpp::FD serverSock;
  unistdpp::FD tcpListenSock;

  ControlServer controlServer;

  pid_t frontPID = 0;
  // Whether this server's own swtcon is currently suspended in favor of
  // an xochitl client that owns the panel (set alongside every
  // suspendForXochitl() call, cleared alongside every resumeForXochitl()
  // call). Needed because a paused-and-owning xochitl client can vanish
  // (crash/disconnect) without ever going through pause() - see
  // updateFrontClient() - so relying solely on findClient()-driven
  // pause()/resume() pairing isn't enough to guarantee it gets resumed.
  bool xochitlOwnsPanel = false;
  // Switch target requested while the current front is a busy (non-idle)
  // xochitl client - see requestSwitch(). Deliberately defers the *whole*
  // switch (not just xochitl's freeze) until it reports idle: submitting
  // updates to our own swtcon while its worker thread is suspended is not
  // safe even setting aside the freeze-timing hazard itself - confirmed
  // empirically that display_thread_func's own dispatch gate
  // (nFrameCleanupCursor vs. gate_target) is paced against real panel
  // progress (nLastPannedFrame, written only by the worker thread), so a
  // second queued update while suspended can hang swtcon_wait() forever.
  // Only one switch can be pending at a time; a newer request simply
  // overwrites an older, still-unresolved one.
  std::optional<pid_t> pendingSwitchTarget;
  std::vector<UnixClient> unixClients;
  std::vector<unistdpp::FD> tcpClients;

  SharedFB& fb;

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
    : controlServer(*this), fb(SharedFB::getInstance()) {

    unistdpp::fatalOnError(fb.alloc(), "Failed to allocated FB");

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

  void initSWTCON() const {
    // Call the get or create Instance function.
    if (!inQemu) {
      std::cerr << "SWTCON calling init\n";

      auto copyBuffer = std::make_unique<std::array<uint8_t, fb_size>>();

      // The init threads does a memset to 0xff. But if we're activated by a
      // systemd socket the shared memory already has some content. So make a
      // backup and preserve it.
      memcpy(copyBuffer.get(), fb.getFb(), fb_size);
      hookAddrs->initThreads();
      memcpy(fb.getFb(), copyBuffer.get(), fb_size);

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
      unixClients, [&](const auto& client) { return !client.sock.isValid(); });

    auto removedTcpClients =
      erase_if(tcpClients, [&](const auto& sock) { return !sock.isValid(); });

    return removedUnixClients != 0 || removedTcpClients != 0;
  }

  void resume(UnixClient& client) {
    frontPID = client.pid;
    std::cerr << "Resuming: " << frontPID << "\n";

    bool shouldUpdate = true;
    if (client.memFD.isValid()) {
      // Set FD and overwrite mapping.
      fb.setFD(std::move(client.memFD));
      fb.mmap();

    } else if (!fb.isValid()) {
      // Fresh client with no buffer of its own yet, and nothing currently
      // live either (e.g. pause() just took the previous front's FD away,
      // or this switch was deferred and never touched fb at all until
      // now - see requestSwitch()) - give it a blank one to draw into.
      if (auto err = fb.alloc(); !err.has_value()) {
        std::cerr << "Error alloc FB: " << to_string(err.error()) << "\n";
      }
      shouldUpdate = !client.isXochitl;
    }

    if (shouldUpdate) {
      doUpdate(UpdateParams{
        .y1 = 0,
        .x1 = 0,
        .y2 = fb_height - 1,
        .x2 = fb_width - 1,
        .flags = 1, // Set sync flag to ensure clean FB.
        .waveform = WAVEFORM_MODE_GC16 | UpdateParams::ioctl_waveform_flag,
        .temperatureOverride = 0,
        .extraMode = 0,
      });
    }

    // If this client's Init message arrived while it wasn't front yet
    // (requestSwitch()'s deferred-switch case), it's still waiting on a
    // reply - send it now that fb is finally set up correctly for it.
    if (client.initPending) {
      client.initPending = false;
      fb.send(client.sock).or_else([&](auto err) {
        std::cerr << "Error sending fb to resumed client: " << to_string(err)
                  << "\n";
      });
    }

    if (!client.dontPause) {
      inputMonitor.flood();
    }

    // client is about to own the panel via its own swtcon instance - the
    // doUpdate() above (through this server's own swtcon, still active at
    // this point) already resynced the panel to client's buffer content and
    // (now that it's Sync) is guaranteed to have fully completed, so it's
    // safe to step this server's own swtcon out of the way before handing
    // control over. Gated on !inQemu like initSWTCON()/doUpdate() - in
    // qemu there's no real swtcon instance here to suspend (hookAddrs
    // ->initThreads() is never called either), so calling into it would
    // touch synchronization objects swtcon_init() never initialized.
    if (client.isXochitl && !inQemu) {
      hookAddrs->suspendForXochitl();
      xochitlOwnsPanel = true;
    }

    kill(-getpgid(frontPID), SIGCONT);
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

    int signal = SIGSTOP;
    if (it->dontPause) {
      std::cerr << "USR1: " << pid << "\n";
      signal = SIGUSR1;
    } else {
      std::cerr << "pausing: " << pid << "\n";
    }

    auto res = kill(-getpgid(pid), signal);
    if (res == -1) {
      perror("Error pausing!");
    }

    it->memFD = fb.takeFD();

    // pid no longer owns the panel - bring this server's own swtcon back
    // so the next resume()'s doUpdate() has something to drive updates
    // through. Gated on !inQemu - see resume()'s matching suspendForXochitl
    // call for why.
    if (it->isXochitl && !inQemu) {
      hookAddrs->resumeForXochitl();
      xochitlOwnsPanel = false;
    }

    return true;
  }

  // Single entry point for switching the front client - acceptUnixClient()
  // and switchTo() both funnel through this instead of calling pause()/
  // resume() directly. If the current front is a busy (non-idle) xochitl
  // client, defers the *entire* switch (not just its freeze) until it
  // reports idle via IdleUpdate - see pendingSwitchTarget's comment for
  // why nothing here can be done partially/early in that case. Otherwise
  // behaves exactly like the old inline pause()+resume() sequence.
  void requestSwitch(pid_t targetPid) {
    // Verify the target still exists BEFORE touching the current front at
    // all - pause(frontPID) below has real, only-partially-reversible
    // side effects (SIGSTOP, buffer detach, resumeForXochitl()). Checking
    // this only *after* calling pause() (an earlier version of this
    // function did) left the front stopped with nothing resumed to take
    // its place whenever a deferred switch's target had vanished by the
    // time it finally resolved - confirmed on the emulator: xochitl stuck
    // in SIGSTOP state after its pending switch target crashed before it
    // ever went idle.
    auto targetIt = findClient(targetPid);
    if (targetIt == unixClients.end()) {
      std::cerr << "requestSwitch: target " << targetPid << " gone\n";
      return;
    }

    if (frontPID != 0) {
      auto frontIt = findClient(frontPID);
      if (frontIt != unixClients.end() && frontIt->isXochitl &&
          !frontIt->idle) {
        pendingSwitchTarget = targetPid;
        std::cerr << "Deferring switch to " << targetPid
                  << ", xochitl still busy\n";
        return;
      }

      pause(frontPID);
    }

    resume(*targetIt);
  }

  unistdpp::Result<std::vector<Client>> getClients() override {
    std::vector<Client> clients;
    std::transform(unixClients.begin(),
                   unixClients.end(),
                   std::back_inserter(clients),
                   [this](const auto& client) {
                     auto res = Client{
                       .pid = client.pid,
                       .active = client.pid == frontPID,
                       .name = {},
                     };
                     auto name = getProcName(client.pid).value_or("<error>");
                     strncpy(res.name, name.data(), sizeof(res.name));
                     return res;
                   });
    return clients;
  }

  unistdpp::Result<int> getFramebuffer(pid_t pid) override {
    if (pid == frontPID) {
      return fb.getFd();
    }

    auto it = findClient(pid);
    if (it == unixClients.end()) {
      return tl::unexpected(std::errc::bad_file_descriptor);
    }

    return it->memFD.fd;
  }

  // Returns success once the switch is *requested*, not necessarily once
  // it's complete - it may be deferred (see requestSwitch()) if the
  // current front is a busy xochitl client. Matches how a fresh
  // acceptUnixClient() connection already behaves (fire-and-forget), and
  // avoids blocking this control-socket RPC (and so callers like
  // rm2fbctl) for an unbounded time.
  unistdpp::Result<void> switchTo(pid_t pid) override {
    if (pid == frontPID) {
      return {};
    }

    auto it = findClient(pid);
    if (it == unixClients.end()) {
      return tl::unexpected(std::errc::bad_file_descriptor);
    }

    requestSwitch(pid);
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
    // If the client we were deferring a switch to has disconnected before
    // ever becoming front, it's not just moot - it can also be the pid a
    // still-connected, still-waiting client (initPending=true) was
    // silently overwritten by (see requestSwitch()'s "newer overwrites
    // older" comment). Promote that waiting client instead of just
    // dropping the request, so it isn't stranded forever with no future
    // event left to ever resume it. requestSwitch() itself decides whether
    // this can resolve immediately (front already idle/gone) or has to
    // defer again (front still busy) - same as any other switch request.
    if (pendingSwitchTarget &&
        findClient(*pendingSwitchTarget) == unixClients.end()) {
      pendingSwitchTarget.reset();
      auto waiting = std::find_if(unixClients.begin(),
                                  unixClients.end(),
                                  [](auto& c) { return c.initPending; });
      if (waiting != unixClients.end()) {
        requestSwitch(waiting->pid);
      }
    }

    if (std::any_of(unixClients.begin(),
                    unixClients.end(),
                    [this](auto& client) { return client.pid == frontPID; })) {
      return;
    }

    // The front client is gone from unixClients (socket closed, dropped by
    // dropClients()) without ever going through pause() - if it was an
    // xochitl client that owned the panel, this server's own swtcon is
    // still suspended and needs to be brought back before anything below
    // tries to doUpdate() through it again.
    if (xochitlOwnsPanel) {
      hookAddrs->resumeForXochitl();
      xochitlOwnsPanel = false;
    }

    // Whatever we were waiting on it to go idle for is moot now - it's
    // gone, not just paused.
    pendingSwitchTarget.reset();

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

    // A live, not-yet-pruned stale entry for this same pid (e.g. a fast
    // reconnect from the same client before dropClients() noticed the old
    // socket's HUP, or a pid reused by an unrelated process before that
    // happened) would otherwise alias findClient(pid)'s first-match lookup
    // - used by requestSwitch() below - onto the OLD entry instead of the
    // one about to be added. Close its socket and run it through the same
    // dropClients()/updateFrontClient() path a real disconnect goes
    // through (resumeForXochitl() if it owned the panel, promoting a
    // waiting pendingSwitchTarget, etc.) before adding the new one, rather
    // than duplicating that cleanup here.
    for (auto& client : unixClients) {
      if (client.pid == peerCred.pid) {
        client.sock.close();
      }
    }
    if (dropClients()) {
      updateFrontClient();
    }

    bool isXochitl = getProcName(peerCred.pid).value_or("") == "xochitl";

    unixClients.emplace_back(UnixClient{
      .sock = std::move(*sock),
      .pid = peerCred.pid,
      .dontPause = false,
      .isXochitl = isXochitl,

      .memFD = FD{},
    });

    // Register first (above), so requestSwitch() can find this client by
    // pid - it handles everything else (pausing the old front if any,
    // buffer handoff, frontPID, suspendForXochitl if this new client is
    // xochitl), including deferring all of it if the current front is a
    // busy xochitl client instead of doing any of it here early.
    requestSwitch(peerCred.pid);

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
    recvMessage<UnixClientMsg>(client.sock)
      .and_then([&](auto msg) -> unistdpp::Result<void> {
        return std::visit(
          [&](auto& m) -> unistdpp::Result<void> {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, Init>) {
              std::cerr << "Got init check!\n";
              if (client.pid != frontPID) {
                // Not front yet - a deferred switch (requestSwitch()) is
                // holding this client back until the current front
                // (necessarily a busy xochitl client) goes idle. Hold the
                // reply too - fb is still the outgoing front's, not this
                // client's - resume() sends it once this client actually
                // becomes front.
                client.initPending = true;
                return {};
              }
              return fb.send(client.sock);
            } else if constexpr (std::is_same_v<T, UpdateParams>) {
              if (client.pid != frontPID) {
                // Only the front client's updates should reach the shared
                // swtcon instance. It may currently be suspended (xochitl
                // owns the panel - see suspendForXochitl()), in which case
                // swtcon_wait() would block forever and hang this
                // server's single poll() thread for every client.
                return client.sock.writeAll(false);
              }
              bool res = doUpdate(m);
              return client.sock.writeAll(res);
            } else {
              // IdleUpdate.
              if (!client.isXochitl) {
                std::cerr << "Unexpected IdleUpdate on a regular client\n";
                return {};
              }
              client.idle = m.val;

              if (m.val && pendingSwitchTarget && client.pid == frontPID) {
                pid_t target = *pendingSwitchTarget;
                pendingSwitchTarget.reset();
                requestSwitch(target);
              }

              return {};
            }
          },
          msg);
      })
      .or_else([&](auto err) {
        std::cerr << "Unix client fail: " << to_string(err) << "\n";
        if (err == unistdpp::FD::eof_error || err == std::errc::broken_pipe) {
          client.sock.close();
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
      [](const auto& client) { return waitFor(client.sock, Wait::Read); });

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
        unixClients[i].sock.close();
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

  addrs->shutdownThreads();

  return EXIT_SUCCESS;
}
