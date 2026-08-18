#include "Client.h"

#include "IOCTL.h"
#include "Versions/Version.h"
#include "rm2fb/SharedBuffer.h"

#ifndef NO_HOOKING
#include "frida-gum.h"
#endif

#include <dlfcn.h>

#include <csignal>
#include <cstring>
#include <linux/limits.h>
#include <unistd.h>

#include "unistdpp/error.h"

bool inXochitl = false;

namespace {

int
setupHooks() {
  const auto* addrs = getAddresses();
  if (addrs == nullptr) {
    return EXIT_FAILURE;
  }

#ifndef NO_HOOKING
  gum_init_embedded();
#else
  return EXIT_FAILURE;
#endif

  auto result = addrs->installHooks(sendUpdate);
  return result ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Sends Init to make sure the rm2fb server is listening and has started
// the SWTCON.
unistdpp::Result<void>
doInit(Buffer& fb) {
  if (fb.isValid()) {
    return {};
  }

  std::cerr << "Sending init check\n";
  ClientSocket sock;
  if (!sock.isValid()) {
    std::cerr << "Init failed, no server running\n";
    std::exit(EXIT_FAILURE);
  }

  return sock.init(false, default_fb_format, fb);
}

} // namespace

extern "C" {

int
open64(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    auto& fb = getGlobalFrameBuffer();
    unistdpp::fatalOnError(doInit(fb), "init FB failed");
    return fb.getFd();
  }

  if (isInputDevicePath(pathname)) {
    return openInputDeviceOrFail(pathname, flags);
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");

  return func_open(pathname, flags, mode);
}

int
open(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    auto& fb = getGlobalFrameBuffer();
    unistdpp::fatalOnError(doInit(fb), "init FB failed");
    return fb.getFd();
  }

  if (isInputDevicePath(pathname)) {
    return openInputDeviceOrFail(pathname, flags);
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");

  return func_open(pathname, flags, mode);
}

int
close(int fd) {
  if (const auto& fb = getGlobalFrameBuffer();
      fb.isValid() && fd == fb.getFd()) {
    return 0;
  }

  static const auto func_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
  return func_close(fd);
}

int
ioctl(int fd, unsigned long request, char* ptr) {
  if (const auto& fb = getGlobalFrameBuffer();
      fb.isValid() && fd == fb.getFd()) {
    return handleIOCTL(request, ptr);
  }

  static auto funcIoctl =
    (int (*)(int, unsigned long request, ...))dlsym(RTLD_NEXT, "ioctl");

  return funcIoctl(fd, request, ptr);
}

int
__ioctl_time64(int fd, unsigned long int request, char* ptr) { // NOLINT
  if (const auto& fb = getGlobalFrameBuffer();
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

    auto& fb = getGlobalFrameBuffer();
    unistdpp::fatalOnError(doInit(fb), "Error making shared FB");
    unistdpp::fatalOnError(fb.mmap(), "Failed to map FB");

    if (setupHooks() != EXIT_SUCCESS) {
      std::cerr << "Seting up hooks failed\n";
      return EXIT_FAILURE;
    }
  }

  auto* funcMain =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return funcMain(mainFn, argc, argv, init, fini, rtldFini, stackEnd);
};
}
