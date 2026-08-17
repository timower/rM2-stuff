#pragma once

// Server's internals, split out of Server.cpp so they can be constructed
// directly from test/unit/TestRm2fbServer.cpp - not a public API, nothing
// outside rm2fb_server's own sources and its test should include this.

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
#include <csignal>
#include <optional>
#include <sys/mman.h>
#include <type_traits>
#include <unistd.h>
#include <variant>
#include <vector>

#include <systemd/sd-daemon.h>

using namespace unistdpp;

// Polyfill for std::erase_if(vector, pred), added to <vector> in C++20 -
// this target still builds as C++17 in some configurations (others now pull
// in C++20 transitively via swtcon), and under C++20 std::erase_if is
// already found via ADL at the unqualified call sites below, ambiguous
// against this same overload - so only define it pre-C++20.
#if __cplusplus < 202002L
template<class T, class Alloc, class Pred>
constexpr typename std::vector<T, Alloc>::size_type
erase_if(std::vector<T, Alloc>& c, Pred pred) {
  auto it = std::remove_if(c.begin(), c.end(), pred);
  auto r = c.end() - it;
  c.erase(it, c.end());
  return r;
}
#endif
// Lets std::visit take one lambda per alternative instead of a single
// generic one with an if constexpr/using-T chain inside - see
// readUnixSock() below.
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

constexpr auto tcp_port = 8888;

// Guards against two rm2fb-server processes running concurrently.
// Deliberately a separate lock file from swtcon's own /tmp/epd.lock (see
// ServerSwtcon.cpp's skipPidLock=true) - that lock is reserved for keeping
// this server's swtcon instance from fighting xochitl's own, independent
// one over the same hardware, coordinated instead via SIGSTOP/SIGCONT
// (suspendForXochitl/resumeForXochitl); it says nothing about whether a
// second rm2fb-server itself is already running, which skipPidLock=true no
// longer guards against on its own. path is overridable so tests don't
// collide with a real, already-running server (or each other).
unistdpp::FD
acquireServerLock(const char* path);

// Starts a tcp server socket
unistdpp::Result<unistdpp::FD>
createTCPSocket(int port);

unistdpp::Result<std::string>
getProcName(pid_t pid);

bool
checkDebugMode();

bool
doTCPUpdate(unistdpp::FD& fd, const Buffer& fb, const UpdateParams& params);

void
handleMsg(const Buffer& fb,
          unistdpp::FD& fd,
          const AllUinputDevices& devs,
          GetUpdate msg);

void
handleMsg(const Buffer& fb,
          unistdpp::FD& fd,
          const AllUinputDevices& devs,
          const Input& msg);

void
handleMsg(const Buffer& fb,
          unistdpp::FD& fd,
          const AllUinputDevices& devs,
          const PowerButton& msg);

// Who's currently driving the physical panel: this server's own swtcon, or
// the front client's independent one (currently only xochitl, via
// ClientSwtcon.cpp/rm2fb_client_swtcon - coordinated via SIGSTOP/SIGCONT +
// AddressInfoBase::suspendForXochitl()/resumeForXochitl() instead of being
// driven through doUpdate()). No client at all is its own third case,
// rather than a magic pid 0.
struct NoFront {};
struct ServerDrivenFront {
  pid_t pid;
};
struct ClientDrivenFront {
  pid_t pid;

  // Switch target requested while this client was busy (non-idle) - see
  // requestSwitch(). Deliberately defers the *whole* switch (not just this
  // client's freeze) until it reports idle: submitting updates to our own
  // swtcon while its worker thread is suspended is not safe even setting
  // aside the freeze-timing hazard itself - confirmed empirically that
  // display_thread_func's own dispatch gate (nFrameCleanupCursor vs.
  // gate_target) is paced against real panel progress (nLastPannedFrame,
  // written only by the worker thread), so a second queued update while
  // suspended can hang swtcon_wait() forever. Only one switch can be
  // pending at a time; a newer request simply overwrites an older,
  // still-unresolved one.
  std::optional<pid_t> pendingSwitchTarget;
};
using FocusState = std::variant<NoFront, ServerDrivenFront, ClientDrivenFront>;

pid_t
focusPid(const FocusState& focus);

// Sends the real SIGSTOP/SIGCONT (or SIGUSR1, for a launcher-exempt
// client - see UnixClient::dontPause) that pause()/resume() use to hand
// panel/input ownership to another process's group - pulled out from
// behind a plain kill() call (like AddressInfoBase's own hooks) so tests
// can swap in a fake that just records what would have been sent,
// instead of needing a real, safely-signalable process to point it at.
struct ProcessControl {
  virtual void pause(pid_t pid, bool dontPause) const = 0;
  virtual void resume(pid_t pid) const = 0;
  virtual ~ProcessControl() = default;
};

struct RealProcessControl : ProcessControl {
  void pause(pid_t pid, bool dontPause) const override {
    int signal = SIGSTOP;
    if (dontPause) {
      std::cerr << "USR1: " << pid << "\n";
      signal = SIGUSR1;
    } else {
      std::cerr << "pausing: " << pid << "\n";
    }
    if (kill(-getpgid(pid), signal) == -1) {
      perror("Error pausing!");
    }
  }

  void resume(pid_t pid) const override { kill(-getpgid(pid), SIGCONT); }
};

// Program-lifetime default so Server's constructor can bind its
// ProcessControl& member to it without dangling - a temporary default
// argument wouldn't outlive the constructor call.
inline const ProcessControl&
defaultProcessControl() {
  static const RealProcessControl instance;
  return instance;
}

struct UnixClient {
  unistdpp::FD sock;
  pid_t pid;
  bool dontPause;

  // Whether this client runs its own, independent swtcon instance -
  // std::nullopt until its Init message is processed (readUnixSock()),
  // deliberately distinct from false: nothing may act on this before it's
  // actually known, since resume()/requestSwitch() decide real panel
  // hand-off based on it.
  std::optional<bool> ownSwtcon;

  // Shared-buffer format granted at Init time (readUnixSock()) - clamped
  // to default_fb_format there unless ownSwtcon is true. Meaningless
  // before ownSwtcon is set, same as ownSwtcon itself.
  FbFormat format = default_fb_format;

  // True once this client has been front at least once. Used to skip
  // resume()'s resync flash the very first time a swtcon-owning client
  // becomes front - no point pushing its blank, freshly-allocated buffer
  // to the panel right before it draws its own real content via its own
  // swtcon.
  bool hasBeenFront = false;

  // Last IdleUpdate value reported by this client (ownSwtcon clients only) -
  // lives here rather than on ClientDrivenFront so it survives across being
  // paused/resumed instead of needing a manual snapshot/restore around every
  // focus change. Gates requestSwitch() while this client is front.
  bool idle = false;

  // This client's own dedicated framebuffer, allocated once when its Init
  // arrives (readUnixSock()) and held for the connection's lifetime. On
  // loan to the shared `fb` (moved out via setFD()) while this client is
  // front; parked back here (moved back via takeFD()) while it isn't -
  // see pause()/resume().
  unistdpp::FD buffer;
};

struct Server : ControlInterface {
  const bool debugMode = checkDebugMode();
  const bool inQemu = !unistdpp::open("/dev/fb0", O_RDONLY).has_value();

  // Held for the process's lifetime once acquired in the constructor -
  // see acquireServerLock().
  unistdpp::FD serverLockFd;

  AllUinputDevices uinputDevices;
  InputMonitor inputMonitor;

  unistdpp::FD serverSock;
  unistdpp::FD tcpListenSock;

  ControlServer controlServer;

  // Which client (if any) currently owns the panel/input, and who's
  // driving it - see FocusState. Single source of truth for focus; every
  // transition goes through pause()/resume()/requestSwitch().
  FocusState focus;

  std::vector<UnixClient> unixClients;
  std::vector<unistdpp::FD> tcpClients;

  Buffer& fb;

  // A spare, unmapped, always-default-format fd - never given to any
  // client. Populated by requestSwitch()'s NoFront branch (rescued from
  // whichever buffer fb happened to be holding that nobody's using
  // anymore, rather than a fresh allocation) and consumed/returned by
  // resume()'s non-default-format resync conversion - see resume().
  unistdpp::FD scratchBuffer;

  const AddressInfoBase* hookAddrs;

  // See ProcessControl's own comment - defaults to actually sending real
  // signals, overridable for tests.
  const ProcessControl& processControl;

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
        serverSock = unistdpp::FD{ fd };
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
        controlServer.sock = unistdpp::FD{ fd };
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
        tcpListenSock = unistdpp::FD{ fd };
        continue;
      }
    }
  }

  void initServerSocket(const char* socketAddr) {
    if (serverSock.isValid()) {
      std::cerr << "Using server socket from systemd\n";
      return;
    }

    // Setup server socket.
    if (unlink(socketAddr) != 0) {
      perror("Socket unlink");
    }

    serverSock =
      unistdpp::fatalOnError(unistdpp::socket(AF_UNIX, SOCK_STREAM, 0),
                             "Failed to create server socket: ");
    unistdpp::fatalOnError(
      unistdpp::bind(serverSock, unistdpp::Address::fromUnixPath(socketAddr)),
      "Failed to bind server sock");
    unistdpp::fatalOnError(unistdpp::listen(serverSock, 5),
                           "Failed to listen on server sock");
  }

  void initTcpSocket() {
    if (tcpListenSock.isValid()) {
      std::cerr << "Using TCP socket from systemd\n";
      return;
    }
    tcpListenSock = createTCPSocket(tcp_port)
                      .or_else([](auto err) {
                        std::cerr << "Error creating TCP socket: "
                                  << unistdpp::to_string(err) << "\n";
                      })
                      .value_or(unistdpp::FD());
  }

  // lockPath/sockPath/processControl default to the real production
  // paths/behavior - overridable so tests can run their own Server
  // instance without colliding with a real rm2fb-server (or each other),
  // or sending real signals to a process they don't control.
  explicit Server(
    const AddressInfoBase* addrs,
    const char* lockPath = "/tmp/rm2fb-server.lock",
    const char* sockPath = default_sock_addr.data(),
    const ProcessControl& processControl = defaultProcessControl())
    : serverLockFd(acquireServerLock(lockPath))
    , controlServer(*this)
    , fb(getGlobalFrameBuffer())
    , processControl(processControl) {

    unistdpp::fatalOnError(fb.alloc(), "Failed to allocated FB");

    getSystemdSockets();
    initServerSocket(sockPath);
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

  bool doUpdateBatch(const std::vector<UpdateParams>& updates) {
    bool res = false;
    if (!inQemu) {
      res = hookAddrs->doUpdateBatch(updates);
    }
    for (auto& msg : updates) {
      for (auto& client : tcpClients) {
        doTCPUpdate(client, fb, msg);
      }

      if (debugMode) {
        std::cerr << "UPDATE " << msg << ": " << res << "\n";
      }
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

  // Only ever called on a client whose Init has already been processed
  // (readUnixSock() is the only place that ever calls requestSwitch(),
  // directly or indirectly - see its comment) - client.ownSwtcon and
  // client.buffer are always already known/populated by the time this
  // runs.
  void resume(UnixClient& client) {
    std::cerr << "Resuming: " << client.pid << "\n";

    // Skip the resync flash only the very first time a swtcon-owning
    // client becomes front - it's about to draw its own real content via
    // its own swtcon immediately after taking over, so pushing its blank,
    // freshly-allocated buffer to the panel first is pure waste. Every
    // other case (including this same client resuming again after a
    // pause()) needs the resync, to make sure the panel actually matches
    // this client's buffer content before it takes back control.
    bool skipResync = client.ownSwtcon == true && !client.hasBeenFront;

    auto resyncUpdate = [&] {
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
    };

    // This server's own swtcon can only decode the default RGB565
    // format (doc/swtcon_3.27_diff.md) - it's bound once, permanently,
    // to whatever address fb.getFb() has, and fb's later mmap() calls
    // keep reusing that same address (MAP_FIXED) so normal updates stay
    // zero-copy. For a granted non-default format, borrow the spare
    // scratchBuffer as fb's backing instead, convert this client's real
    // content into it (never touching the client's own buffer), flash
    // that, then return scratchBuffer for next time before finally
    // handing fb the client's real buffer below.
    bool needsConversion =
      !skipResync && client.format.pixelFormat != PixelFormat::RGB565;
    if (needsConversion) {
      fb.setFD(std::move(scratchBuffer), default_fb_format);
      fb.mmap();
      if (auto src = unistdpp::mmap(nullptr,
                                    client.format.totalSize(),
                                    PROT_READ,
                                    MAP_SHARED,
                                    client.buffer,
                                    0)) {
        convertToRGB565(client.format, src->get(), (uint16_t*)fb.getFb());
      }
      resyncUpdate();
      scratchBuffer = fb.takeFD();
    }

    fb.setFD(std::move(client.buffer), client.format);
    fb.mmap();

    if (!skipResync && !needsConversion) {
      resyncUpdate();
    }

    if (!client.dontPause) {
      inputMonitor.flood();
    }

    // client is about to own the panel via its own swtcon instance - the
    // doUpdate() above (through this server's own swtcon, still active at
    // this point) already resynced the panel to client's buffer content and
    // (now that it's Sync) is guaranteed to have fully completed, so it's
    // safe to step this server's own swtcon out of the way before handing
    // control over. focus tracks client-driven-ness unconditionally
    // (it's just bookkeeping), but the actual hookAddrs call is gated on
    // !inQemu like initSWTCON()/doUpdate() - in qemu there's no real
    // swtcon instance here to suspend (hookAddrs->initThreads() is never
    // called either), so calling into it would touch synchronization
    // objects swtcon_init() never initialized.
    if (client.ownSwtcon == true) {
      if (!inQemu) {
        hookAddrs->suspendForXochitl();
      }
      focus = ClientDrivenFront{ .pid = client.pid,
                                 .pendingSwitchTarget = std::nullopt };
    } else {
      focus = ServerDrivenFront{ client.pid };
    }

    client.hasBeenFront = true;

    processControl.resume(client.pid);
  }

  auto findClient(pid_t pid) {
    return std::find_if(
      unixClients.begin(), unixClients.end(), [pid](const auto& client) {
        return client.pid == pid;
      });
  }

  // Only ever called with pid == focusPid(focus) - see requestSwitch().
  bool pause(pid_t pid) {
    auto it = findClient(pid);
    if (it == unixClients.end()) {
      std::cerr << "No client found with paused pid\n";
      return false;
    }

    processControl.pause(pid, it->dontPause);

    it->buffer = fb.takeFD();

    // pid no longer owns the panel - bring this server's own swtcon back
    // so the next resume()'s doUpdate() has something to drive updates
    // through. Gated on !inQemu - see resume()'s matching suspendForXochitl
    // call for why.
    if (std::holds_alternative<ClientDrivenFront>(focus) && !inQemu) {
      hookAddrs->resumeForXochitl();
    }

    return true;
  }

  // Single entry point for switching the front client - only ever called
  // from readUnixSock()'s Init/IdleUpdate handling, switchTo() and
  // updateFrontClient() - instead of calling pause()/resume() directly.
  // If the current front is a busy (non-idle) client-driven front, defers
  // the *entire* switch (not just its freeze) until it reports idle via
  // IdleUpdate - see ClientDrivenFront::pendingSwitchTarget's comment for
  // why nothing here can be done partially/early in that case. Otherwise
  // behaves exactly like the old inline pause()+resume() sequence.
  void requestSwitch(pid_t targetPid) {
    // Verify the target has actually completed Init (ownSwtcon known,
    // buffer allocated - see readUnixSock()) BEFORE touching the current
    // front at all - pause(frontPid) below has real, only-partially-
    // reversible side effects (SIGSTOP, buffer detach, resumeForXochitl()).
    // Checking this only *after* calling pause() (an earlier version of
    // this function did) left the front stopped with nothing resumed to
    // take its place whenever a deferred switch's target had vanished by
    // the time it finally resolved - confirmed on the emulator: xochitl
    // stuck in SIGSTOP state after its pending switch target crashed
    // before it ever went idle.
    auto targetIt = findClient(targetPid);
    if (targetIt == unixClients.end() || !targetIt->ownSwtcon.has_value()) {
      std::cerr << "requestSwitch: target " << targetPid << " not ready\n";
      return;
    }

    if (auto* front = std::get_if<ClientDrivenFront>(&focus)) {
      auto frontIt = findClient(front->pid);
      bool idle = frontIt != unixClients.end() && frontIt->idle;
      if (!idle) {
        front->pendingSwitchTarget = targetPid;
        std::cerr << "Deferring switch to " << targetPid
                  << ", client still busy\n";
        return;
      }
    }

    if (pid_t frontPid = focusPid(focus); frontPid != 0) {
      pause(frontPid);
    } else if (std::holds_alternative<NoFront>(focus)) {
      // fb currently holds a buffer nobody's using (the server's own
      // startup buffer the first time this fires; a disconnected
      // client's leftover buffer on any later NoFront) that resume()
      // below would otherwise just close via fb.setFD() - rescue it
      // into scratchBuffer instead, as a private, never-client-visible
      // scratch target for the resync conversion flash (its content is
      // always fully overwritten before use, so it doesn't matter whose
      // leftover buffer it happens to be).
      scratchBuffer = fb.takeFD();
    }

    resume(*targetIt);
  }

  unistdpp::Result<std::vector<Client>> getClients() override {
    std::vector<Client> clients;
    pid_t frontPid = focusPid(focus);
    std::transform(unixClients.begin(),
                   unixClients.end(),
                   std::back_inserter(clients),
                   [frontPid](const auto& client) {
                     auto res = Client{
                       .pid = client.pid,
                       .active = client.pid == frontPid,
                       .format = client.format,
                       .name = {},
                     };
                     auto name = getProcName(client.pid).value_or("<error>");
                     strncpy(res.name, name.data(), sizeof(res.name));
                     return res;
                   });
    return clients;
  }

  unistdpp::Result<int> getFramebuffer(pid_t pid) override {
    if (pid == focusPid(focus)) {
      return fb.getFd();
    }

    auto it = findClient(pid);
    if (it == unixClients.end()) {
      return tl::unexpected(std::errc::bad_file_descriptor);
    }

    return it->buffer.fd;
  }

  // Returns success once the switch is *requested*, not necessarily once
  // it's complete - it may be deferred (see requestSwitch()) if the
  // current front is a busy client-driven front. Matches how a fresh
  // connection's own Init already behaves (fire-and-forget), and avoids
  // blocking this control-socket RPC (and so callers like rm2fbctl) for an
  // unbounded time.
  unistdpp::Result<void> switchTo(pid_t pid) override {
    if (pid == focusPid(focus)) {
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
    // still-connected, still-waiting client (hasBeenFront == false) was
    // silently overwritten by (see requestSwitch()'s "newer overwrites
    // older" comment). Promote that waiting client instead of just
    // dropping the request, so it isn't stranded forever with no future
    // event left to ever resume it. requestSwitch() itself decides whether
    // this can resolve immediately (front already idle/gone) or has to
    // defer again (front still busy) - same as any other switch request.
    if (auto* front = std::get_if<ClientDrivenFront>(&focus);
        front && front->pendingSwitchTarget &&
        findClient(*front->pendingSwitchTarget) == unixClients.end()) {
      front->pendingSwitchTarget.reset();
      auto waiting =
        std::find_if(unixClients.begin(), unixClients.end(), [](auto& c) {
          return c.ownSwtcon.has_value() && !c.hasBeenFront;
        });
      if (waiting != unixClients.end()) {
        requestSwitch(waiting->pid);
      }
    }

    pid_t frontPid = focusPid(focus);
    if (std::any_of(
          unixClients.begin(), unixClients.end(), [frontPid](auto& client) {
            return client.pid == frontPid;
          })) {
      return;
    }

    // The front client is gone from unixClients (socket closed, dropped by
    // dropClients()) without ever going through pause() - if it was a
    // client-driven front, this server's own swtcon is still suspended and
    // needs to be brought back before anything below tries to doUpdate()
    // through it again.
    if (std::holds_alternative<ClientDrivenFront>(focus) && !inQemu) {
      hookAddrs->resumeForXochitl();
    }

    focus = NoFront{};

    // Promote the most recently connected client that's actually ready
    // (Init processed - see requestSwitch()); one that isn't yet will
    // trigger its own requestSwitch() once its Init arrives.
    auto next = std::find_if(unixClients.rbegin(),
                             unixClients.rend(),
                             [](auto& c) { return c.ownSwtcon.has_value(); });
    if (next == unixClients.rend()) {
      return;
    }

    std::cerr << "Front client gone ";
    resume(*next);
  }

  bool acceptUnixClient() {
    auto sock = unistdpp::accept(serverSock, nullptr, nullptr);
    if (!sock) {
      std::cerr << "Unix client accept error: "
                << unistdpp::to_string(sock.error()) << "\n";
      return false;
    }

    ucred peerCred;
    socklen_t len = sizeof(peerCred);
    if (auto err =
          unistdpp::getsockopt(*sock, SOL_SOCKET, SO_PEERCRED, &peerCred, &len);
        !err) {
      std::cerr << "Error getting peercred: "
                << unistdpp::to_string(err.error()) << "\n";
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

    unixClients.emplace_back(UnixClient{
      .sock = std::move(*sock),
      .pid = peerCred.pid,
      .dontPause = false,
      .ownSwtcon = std::nullopt,
      .buffer = unistdpp::FD{},
    });

    // Just register the client - it isn't ready to become front yet, and
    // won't be until its Init tells us whether it owns its own swtcon and
    // gives it a buffer to draw into (see readUnixSock()).
    return true;
  }

  bool acceptTcpClient() {
    auto sock = unistdpp::accept(tcpListenSock, nullptr, nullptr);
    if (!sock) {
      std::cerr << "Client accept errror: " << unistdpp::to_string(sock.error())
                << "\n";
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
          overloaded{
            [&](Init& m) -> unistdpp::Result<void> {
              std::cerr << "Got init check!\n";

              client.ownSwtcon = m.ownSwtcon;
              client.format = clampToSupported(m.format, m.ownSwtcon);

              auto buf = allocBlankBuffer(client.format);
              if (!buf) {
                // No usable buffer for this client to ever become front
                // with - drop it instead of replying, rather than let
                // requestSwitch() below promote a client with no buffer.
                std::cerr << "Error allocating client buffer: "
                          << unistdpp::to_string(buf.error())
                          << " - dropping client\n";
                client.sock.close();
                return {};
              }
              client.buffer = std::move(*buf);

              // Reply with the granted format followed by this client's
              // own dedicated buffer right away instead of waiting for
              // it to actually become front (that used to happen in
              // resume(), gated on hasBeenFront) - a client can mmap and
              // even pre-render into it immediately; it just can't push
              // real panel updates until requestSwitch() below actually
              // promotes it (see the UpdateParams handling further down,
              // which drops updates from a non-front client).
              client.sock.writeAll(client.format)
                .and_then([&] {
                  return unistdpp::sendFDTo(client.sock, client.buffer.fd);
                })
                .or_else([&](auto err) {
                  std::cerr << "Error sending fb to client: "
                            << unistdpp::to_string(err) << "\n";
                });

              // requestSwitch() handles everything else (pausing the old
              // front if any, buffer handoff, suspendForXochitl if this
              // client owns its own swtcon) - including deferring all of
              // it if the current front is a busy client-driven one,
              // instead of doing any of it here.
              requestSwitch(client.pid);
              return {};
            },
            [&](UpdateParams& m) -> unistdpp::Result<void> {
              if (client.pid != focusPid(focus)) {
                // Only the front client's updates should reach the shared
                // swtcon instance. It may currently be suspended (a
                // client-driven front owns the panel - see
                // suspendForXochitl()), in which case swtcon_wait() would
                // block forever and hang this server's single poll()
                // thread for every client.
                return client.sock.writeAll(false);
              }
              bool res = doUpdate(m);
              return client.sock.writeAll(res);
            },
            [&](UpdateBatchHeader& m) -> unistdpp::Result<void> {
              // Must drain the payload regardless of front-client status
              // below, or the next recvMessage() on this socket desyncs.
              std::vector<UpdateParams> updates(m.count);
              if (m.count > 0) {
                TRY(client.sock.readAll(updates.data(),
                                        updates.size() * sizeof(UpdateParams)));
              }

              if (client.pid != focusPid(focus)) {
                // See the UpdateParams handler above for why non-front
                // updates are dropped.
                return client.sock.writeAll(false);
              }
              bool res = doUpdateBatch(updates);
              return client.sock.writeAll(res);
            },
            [&](IdleUpdate& m) -> unistdpp::Result<void> {
              auto* front = std::get_if<ClientDrivenFront>(&focus);
              if (!front || front->pid != client.pid) {
                if (client.ownSwtcon != true) {
                  std::cerr << "Unexpected IdleUpdate on a regular client\n";
                }
                return {};
              }

              client.idle = m.val;

              if (m.val && front->pendingSwitchTarget) {
                pid_t target = *front->pendingSwitchTarget;
                front->pendingSwitchTarget.reset();
                requestSwitch(target);
              }

              return {};
            },
          },
          msg);
      })
      .or_else([&](auto err) {
        std::cerr << "Unix client fail: " << unistdpp::to_string(err) << "\n";
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
        std::cerr << "Reading input: " << unistdpp::to_string(err) << "\n";
        if (err == unistdpp::FD::eof_error) {
          sock.close();
        }
      });
  }

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
      std::cerr << "Poll error: " << unistdpp::to_string(res.error()) << "\n";
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
        std::cerr << "Control error: " << unistdpp::to_string(err) << "\n";
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
