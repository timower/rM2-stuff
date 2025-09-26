#include "Client.h"

#include "ControlSocket.h"
#include "IOCTL.h"
#include "SharedBuffer.h"
#include "Versions/Version.h"

#ifndef NO_HOOKING
#include "frida-gum.h"
#endif

#include <dlfcn.h>

#include <cstring>
#include <linux/limits.h>
#include <unistd.h>

bool inXochitl = false;

namespace {

const unistdpp::Result<ControlSocket>&
getControlSocket() {
  static auto socket = []() -> unistdpp::Result<ControlSocket> {
    ControlSocket res;
    TRY(res.init(nullptr));
    TRY(res.connect(default_sock_addr.data()));
    return res;
  }();

  return socket;
}

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

// Sends an empty message to make sure the rm2fb server is listening and has
// started the SWTCON.
void
waitForInit() {
  std::cerr << "Sending init check\n";
  sendUpdate(UpdateParams{
    .y1 = 0,
    .x1 = 0,
    .y2 = 0,
    .x2 = 0,
    .flags = 0,
    .waveform = 0,
    .temperatureOverride = 0,
    .extraMode = 0,
  });
}

} // namespace

bool
sendUpdate(const UpdateParams& params) {
  const auto& clientSock = unistdpp::fatalOnError(getControlSocket());

  return clientSock.sendto(params)
    .and_then([&](auto _) {
      return clientSock.recvfrom<bool>().map(
        [](auto pair) { return pair.first; });
    })
    .value_or(false);
}

extern "C" {

int
open64(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
    waitForInit();
    return fb.fd.fd;
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");

  return func_open(pathname, flags, mode);
}

int
open(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
    waitForInit();
    return fb.fd.fd;
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");

  return func_open(pathname, flags, mode);
}

int
close(int fd) {
  if (const auto& fb = SharedFB::getInstance();
      fb.has_value() && fd == fb->fd.fd) {
    return 0;
  }

  static const auto func_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
  return func_close(fd);
}

int
ioctl(int fd, unsigned long request, char* ptr) {
  if (const auto& fb = SharedFB::getInstance();
      !inXochitl && fb.has_value() && fd == fb->fd.fd) {
    return handleIOCTL(request, ptr);
  }

  static auto funcIoctl =
    (int (*)(int, unsigned long request, ...))dlsym(RTLD_NEXT, "ioctl");

  return funcIoctl(fd, request, ptr);
}

int
__ioctl_time64(int fd, unsigned long int request, char* ptr) { // NOLINT
  if (const auto& fb = SharedFB::getInstance();
      !inXochitl && fb.has_value() && fd == fb->fd.fd) {
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

  char pathBuffer[PATH_MAX];
  auto size = readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer, size) == "/usr/bin/xochitl") {
    inXochitl = true;

    unistdpp::fatalOnError(SharedFB::getInstance(), "Error making shared FB");
    unistdpp::fatalOnError(getControlSocket(), "Error creating control socket");

    if (setupHooks() != EXIT_SUCCESS) {
      std::cerr << "Seting up hooks failed\n";
      return EXIT_FAILURE;
    }
    waitForInit();
  }

  auto* funcMain =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return funcMain(mainFn, argc, argv, init, fini, rtldFini, stackEnd);
};
}
