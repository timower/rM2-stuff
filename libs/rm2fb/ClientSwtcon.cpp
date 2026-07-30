// rm2fb client, xochitl-coexistence variant: lets xochitl run its own
// (statically-linked) swtcon completely untouched instead of hooking its
// internal update/init/shutdown calls by address per xochitl version (see
// Client.cpp/Versions/*.cpp, kept unchanged for backwards compat - older
// xochitl builds that still dlopen libqsgepaper.so need that path).
//
// Two independent pieces, both validated standalone in
// tools/xochitl-preload before landing here:
//
//  1. For xochitl specifically: malloc/calloc hooks (PreloadHooks.h/.cpp),
//     installed only once inXochitl is confirmed (see __libc_start_main
//     below - installing them any earlier, e.g. from a global constructor,
//     would apply them to every process this library is preloaded into),
//     redirect its own internal 16-bit working buffer / grayscale back
//     buffer allocations into a SharedFB received from the rm2fb server,
//     matched by exact size (fb_size/grayscale_size - a fixed hardware
//     size, not per-xochitl-version) - and an ioctl hook that relays every
//     blank/unblank transition to the server as an IdleUpdate message
//     (rm2fb/Message.h) so it knows when it's safe to pause xochitl. The
//     handful of spurious transitions during the one-time boot flash
//     (which blanks *and* unblanks the panel as intermediate steps) are
//     sent too rather than filtered by a startup grace window - see
//     handleBlankTransition's comment for why that's fine.
//  2. For every other client (unchanged from Client.cpp): open()/ioctl()
//     redirect a virtual /dev/fb0 to the shared framebuffer.
//
// Every message on the control-socket connection (both cases above) is a
// tagged UnixClientMsg (rm2fb/Message.h) - Init first, always, then either
// IdleUpdate (xochitl only) or UpdateParams (everyone else) - rather than
// a bare untagged UpdateParams write with a degenerate empty-rect sentinel
// standing in for "this one's actually an init request." xochitl never
// sends UpdateParams here at all - its own swtcon drives the real panel
// directly; the malloc/calloc-redirected buffer just gives the server a
// live view of its content for when it needs to redraw it (see
// Server.cpp's pause()/resume(), swtcon_suspend/resume in swtcon.h).

#include "IOCTL.h"
#include "PreloadHooks.h"
#include "rm2fb/Message.h"
#include "rm2fb/SharedBuffer.h"

#include <unistdpp/error.h>
#include <unistdpp/socket.h>

#include <cstdint>
#include <cstring>
#include <csignal>
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <linux/fb.h>
#include <linux/limits.h>
#include <unistd.h>

bool inXochitl = false; // NOLINT

namespace {

// Shared by both the generic (non-xochitl) redirect path below and
// xochitlFb()'s handshake - identical to Client.cpp's own
// getControlSocket()/doInit(), duplicated rather than shared across the
// two client libraries so this one has zero dependency on Client.cpp/
// Versions.cpp/Frida.
unistdpp::FD&
getControlSocket() {
  static unistdpp::FD res;
  if (!res.isValid()) {
    res = unistdpp::fatalOnError(
      unistdpp::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));

    unistdpp::bind(res, unistdpp::Address::fromUnixPath(nullptr))
      .and_then([] {
        return unistdpp::connect(
          res, unistdpp::Address::fromUnixPath(default_sock_addr.data()));
      })
      .or_else([](auto err) {
        std::cerr << "Failed connecting to rm2fb: " << unistdpp::to_string(err)
                  << "\n";
        res.close();
      });
  }
  return res;
}

// Sends Init to make sure the rm2fb server is listening and has started
// the SWTCON, then receives the shared framebuffer.
unistdpp::Result<void>
doInit(SharedFB& fb) {
  if (fb.isValid()) {
    return {};
  }

  auto& sock = getControlSocket();
  if (!sock.isValid()) {
    std::cerr << "Init failed, no server running\n";
    std::exit(EXIT_FAILURE);
  }

  return sendMessage(sock, UnixClientMsg{ Init{} })
    .and_then([&] { return fb.recv(sock); })
    .or_else([&](auto err) {
      std::cerr << "Error sending: " << unistdpp::to_string(err) << "\n";
      sock.close();
    });
}

// ---------------------------------------------------------------------
// xochitl-specific: malloc/calloc redirect + blank/un-blank tracking.
// Ported as-is from the validated tools/xochitl-preload prototype.
// ---------------------------------------------------------------------

int g_realFbFd = -1;

// Lazily handshaken on first matching allocation - sends the same
// degenerate empty-rect UpdateParams "init check" doInit() uses for
// everyone else, then receives a SharedFB back via fd-passing.
SharedFB&
xochitlFb() {
  static const bool ok = [] {
    auto res = doInit(SharedFB::getInstance());
    if (!res) {
      std::cerr << "rm2fb: failed receiving shared framebuffer\n";
      return false;
    }
    auto mapRes = SharedFB::getInstance().mmap();
    if (!mapRes) {
      std::cerr << "rm2fb: failed mapping shared framebuffer\n";
      return false;
    }
    return true;
  }();
  (void)ok;
  return SharedFB::getInstance();
}

// One-shot: matches xochitl's own 16-bit working-buffer malloc (by exact,
// hardware-fixed size), redirects it into the shared framebuffer, then
// unhooks itself - same pattern as the old Version3.20.cpp mallocHook.
void*
mallocHook(void* (*orig)(size_t), size_t size) {
  if (size == fb_size) {
    auto& fb = xochitlFb();
    if (fb.isValid()) {
      PreloadHook::getInstance().unhook<PreloadHook::Malloc>();
      return fb.getFb();
    }
  }

  return orig(size);
}

// Same, for the grayscale back buffer's calloc. PreloadHooks' HOOKS macro
// names these params (size, count) even though they're forwarded
// positionally as calloc's real (nmemb, elemSize).
void*
callocHook(void* (*orig)(size_t, size_t), size_t nmemb, size_t elemSize) {
  if (nmemb == grayscale_size && elemSize == 1) {
    auto& fb = xochitlFb();
    if (fb.isValid()) {
      PreloadHook::getInstance().unhook<PreloadHook::Calloc>();
      return fb.getGrayBuffer();
    }
  }

  return orig(nmemb, elemSize);
}

// Forwards every genuine blank/unblank transition to the server as-is, with
// no client-side dedup/state tracking (an earlier version tracked
// "currently idle" locally via an atomic to skip redundant sends and to
// filter out the boot flash's own blank/unblank churn behind a startup
// grace window - dropped because the server's IdleUpdate handling
// (Server.cpp's readUnixSock) is already idempotent: it just assigns
// client.idle = m.val and only *acts* on a resulting idle=true once, via
// pendingSwitchTarget's own reset-on-first-use. So repeated/spurious
// transitions - including the flash's - are harmless there, whereas
// committing local dedup state before a send that might fail was a real
// bug: a dropped send left this side and the server permanently out of
// sync with no way to recover except a coincidentally-matching future
// transition. Forwarding unconditionally instead means a failed send is
// naturally retried by whatever real ioctl retry loop caused it (e.g.
// pan_and_unblank's own up-to-5x FBIOBLANK retry), since each retried
// ioctl re-enters this hook.
void
handleBlankTransition(unsigned long request, void* arg) {
  if (request != FBIOBLANK)
    return;

  bool goingIdle;
  if ((uintptr_t)arg == FB_BLANK_POWERDOWN)
    goingIdle = true;
  else if ((uintptr_t)arg == FB_BLANK_UNBLANK)
    goingIdle = false;
  else
    return; // some other blank level (vsync/hsync suspend) - not tracked

  std::cerr << "rm2fb: xochitl " << (goingIdle ? "idle" : "un-idle") << "\n";

  auto& sock = getControlSocket();
  if (!sock.isValid())
    return;

  auto res = sendMessage(sock, UnixClientMsg{ IdleUpdate{ goingIdle } });
  if (!res) {
    std::cerr << "rm2fb: failed sending " << (goingIdle ? "idle" : "un-idle")
              << " notice: " << unistdpp::to_string(res.error()) << "\n";
  }
}

} // namespace

bool
sendUpdate(const UpdateParams& params) {
  auto& clientSock = getControlSocket();
  if (!clientSock.isValid()) {
    return false;
  }

  return sendMessage(clientSock, UnixClientMsg{ params })
    .and_then([&]() { return clientSock.readAll<bool>(); })
    .or_else([&](auto err) {
      std::cerr << "Error sending: " << unistdpp::to_string(err) << "\n";
      clientSock.close();
    })
    .value_or(false);
}

extern "C" {

int
open64(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    auto& fb = SharedFB::getInstance();
    unistdpp::fatalOnError(doInit(fb), "init FB failed");
    return fb.getFd();
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");
  int fd = func_open(pathname, flags, mode);

  if (inXochitl && fd >= 0 && pathname == std::string("/dev/fb0"))
    g_realFbFd = fd;

  return fd;
}

int
open(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    auto& fb = SharedFB::getInstance();
    unistdpp::fatalOnError(doInit(fb), "init FB failed");
    return fb.getFd();
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
  int fd = func_open(pathname, flags, mode);

  if (inXochitl && fd >= 0 && pathname == std::string("/dev/fb0"))
    g_realFbFd = fd;

  return fd;
}

int
close(int fd) {
  if (const auto& fb = SharedFB::getInstance();
      fb.isValid() && fd == fb.getFd()) {
    return 0;
  }

  static const auto func_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
  return func_close(fd);
}

int
ioctl(int fd, unsigned long request, char* ptr) {
  if (inXochitl && fd == g_realFbFd)
    handleBlankTransition(request, ptr);

  if (const auto& fb = SharedFB::getInstance();
      fb.isValid() && fd == fb.getFd()) {
    return handleIOCTL(request, ptr);
  }

  static auto funcIoctl =
    (int (*)(int, unsigned long request, ...))dlsym(RTLD_NEXT, "ioctl");

  return funcIoctl(fd, request, ptr);
}

int
__ioctl_time64(int fd, unsigned long int request, char* ptr) { // NOLINT
  if (inXochitl && fd == g_realFbFd)
    handleBlankTransition(request, ptr);

  if (const auto& fb = SharedFB::getInstance();
      fb.isValid() && fd == fb.getFd()) {
    return handleIOCTL(request, ptr);
  }

  static auto funcIoctl = (int (*)(int, unsigned long request, ...))dlsym(
    RTLD_NEXT, "__ioctl_time64");

  return funcIoctl(fd, request, ptr);
}

constexpr key_t rm2fb_key = 0x2257c;
static int rm2fbMqid = -1;

int
msgget(key_t key, int msgflg) {
  static auto funcMsgsnd = (int (*)(key_t, int))dlsym(RTLD_NEXT, "msgget");
  int res = funcMsgsnd(key, msgflg);
  if (!inXochitl && key == rm2fb_key) {
    rm2fbMqid = res;
  }
  return res;
}

int
msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg) {
  if (!inXochitl && msqid == rm2fbMqid) {
    return handleMsgSend(msgp, msgsz);
  }

  static auto funcMsgsnd =
    (int (*)(int, const void*, size_t, int))dlsym(RTLD_NEXT, "msgsnd");

  return funcMsgsnd(msqid, msgp, msgsz, msgflg);
}
}

extern "C" {

int
setenv(const char* name, const char* value, int overwrite) {
  static const auto original_func =
    (int (*)(const char*, const char*, int))dlsym(RTLD_NEXT, "setenv");

  if (!inXochitl && strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0) {
    value = "rotate=180:invertx";
  }

  return original_func(name, value, overwrite);
}

int
__libc_start_main(int (*mainFn)(int, char**, char**), // NOLINT
                  int argc,
                  char** argv,
                  int (*init)(int, char**, char**),
                  void (*fini)(void),
                  void (*rtldFini)(void),
                  void* stackEnd) {

  setenv("RM2FB_SHIM", "1", 1);
  setenv("RM2STUFF_RM2FB", "1", 1);
  if (getenv("RM2FB_ACTIVE") != nullptr) {
    setenv("RM2FB_NESTED", "1", 1);
  } else {
    setenv("RM2FB_ACTIVE", "1", 1);
  }

  // We don't support waiting with semaphores yet
  setenv("RM2FB_NO_WAIT_IOCTL", "1", 1);

  // Don't kill ourselves when SIGPIPE happens because rm2fb went down.
  // It might come back up later!
  std::signal(SIGPIPE, SIG_IGN);

  char pathBuffer[PATH_MAX];
  auto size = readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer, size) == "/usr/bin/xochitl") {
    inXochitl = true;
    // No per-version hooking here (compare Client.cpp's setupHooks()) -
    // xochitl runs its own swtcon completely untouched. The malloc/calloc
    // hooks, installed here rather than from a global constructor (ld.so
    // runs every preloaded library's constructors before this function -
    // installing them unconditionally there would apply them to every
    // process this library is preloaded into, not just xochitl), and the
    // ioctl blank-tracking hook (gated on inXochitl in the hooks below) do
    // everything lazily, in step with xochitl's own natural startup
    // sequence, instead of front-loading a handshake here.
    PreloadHook::getInstance().hook<PreloadHook::Malloc>(mallocHook);
    PreloadHook::getInstance().hook<PreloadHook::Calloc>(callocHook);
  }

  auto* funcMain =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return funcMain(mainFn, argc, argv, init, fini, rtldFini, stackEnd);
};
}
