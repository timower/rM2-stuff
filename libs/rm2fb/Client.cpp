#include "Client.h"

#include "ControlSocket.h"
#include "IOCTL.h"
#include "SharedBuffer.h"
#include "Versions/Version.h"

#include "frida-gum.h"

#include <dlfcn.h>

#include <cstring>
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

  gum_init_embedded();

  auto result = addrs->installHooks(sendUpdate);
  return result ? EXIT_SUCCESS : EXIT_FAILURE;
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

  static auto func_ioctl =
    (int (*)(int, unsigned long request, ...))dlsym(RTLD_NEXT, "ioctl");

  return func_ioctl(fd, request, ptr);
}

constexpr key_t rm2fb_key = 0x2257c;
static int rm2fb_mqid = -1;

int
msgget(key_t key, int msgflg) {
  static auto func_msgsnd = (int (*)(key_t, int))dlsym(RTLD_NEXT, "msgget");
  int res = func_msgsnd(key, msgflg);
  if (!inXochitl && key == rm2fb_key) {
    rm2fb_mqid = res;
  }
  return res;
}

int
msgsnd(int msqid, const void* msgp, size_t msgsz, int msgflg) {
  if (!inXochitl && msqid == rm2fb_mqid) {
    return handleMsgSend(msgp, msgsz);
  }

  static auto func_msgsnd =
    (int (*)(int, const void*, size_t, int))dlsym(RTLD_NEXT, "msgsnd");

  return func_msgsnd(msqid, msgp, msgsz, msgflg);
}
}

extern "C" {

int
setenv(const char* name, const char* value, int overwrite) {
  static const auto originalFunc =
    (int (*)(const char*, const char*, int))dlsym(RTLD_NEXT, "setenv");

  if (!inXochitl && strcmp(name, "QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS") == 0) {
    value = "rotate=180:invertx";
  }

  return originalFunc(name, value, overwrite);
}

int
__libc_start_main(int (*_main)(int, char**, char**),
                  int argc,
                  char** argv,
                  int (*init)(int, char**, char**),
                  void (*fini)(void),
                  void (*rtld_fini)(void),
                  void* stack_end) {

  setenv("RM2FB_SHIM", "1", true);
  setenv("RM2STUFF_RM2FB", "1", true);
  if (getenv("RM2FB_ACTIVE") != nullptr) {
    setenv("RM2FB_NESTED", "1", true);
  } else {
    setenv("RM2FB_ACTIVE", "1", true);
  }

  // We don't support waiting with semaphores yet
  setenv("RM2FB_NO_WAIT_IOCTL", "1", true);

  char pathBuffer[PATH_MAX];
  auto size = readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer, size) == "/usr/bin/xochitl") {
    inXochitl = true;

    unistdpp::fatalOnError(SharedFB::getInstance(), "Error making shared FB");
    unistdpp::fatalOnError(getControlSocket(), "Error creating control socket");

    if (setupHooks() != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }

  auto* func_main =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
};
}
