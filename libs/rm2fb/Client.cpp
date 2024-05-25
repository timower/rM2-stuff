#include "Client.h"

#include "IOCTL.h"
#include "ImageHook.h"

#include "ControlSocket.h"
#include "Versions/Version.h"

#include "frida-gum.h"

#include <dlfcn.h>

#include <cstring>
#include <iostream>
#include <unistd.h>

namespace {

ControlSocket clientSock;
bool inXochitl = false;

int
setupHooks() {
  const auto* addrs = getAddresses();
  if (!addrs) {
    return EXIT_FAILURE;
  }

  gum_init_embedded();

  auto result = addrs->installHooks(sendUpdate);
  return result ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

bool
sendUpdate(const UpdateParams& params) {
  return clientSock.sendto(params)
    .and_then([](auto _) {
      return clientSock.recvfrom<bool>().map(
        [](auto pair) { return pair.first; });
    })
    .value_or(false);
}

extern "C" {

int
open64(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    return fb.fd;
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");

  return func_open(pathname, flags, mode);
}

int
open(const char* pathname, int flags, mode_t mode = 0) {
  if (!inXochitl && pathname == std::string("/dev/fb0")) {
    return fb.fd;
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");

  return func_open(pathname, flags, mode);
}

int
close(int fd) {
  if (fd == fb.fd) {
    return 0;
  }

  static const auto func_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
  return func_close(fd);
}

int
ioctl(int fd, unsigned long request, char* ptr) {
  if (!inXochitl && fd == fb.fd) {
    return handleIOCTL(request, ptr);
  }

  static auto func_ioctl =
    (int (*)(int, unsigned long request, ...))dlsym(RTLD_NEXT, "ioctl");

  return func_ioctl(fd, request, ptr);
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
  setenv("RM2FB_ACTIVE", "1", true);

  if (fb.mem == nullptr) {
    std::cout << "No rm2fb shared memory\n";
    return EXIT_FAILURE;
  }

  if (!clientSock.init(nullptr)) {
    std::cout << "No rm2fb socket\n";
    return EXIT_FAILURE;
  }
  if (!clientSock.connect(default_sock_addr.data())) {
    std::cout << "No rm2fb socket\n";
    return EXIT_FAILURE;
  }

  char pathBuffer[PATH_MAX];
  auto size = readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer, size) == "/usr/bin/xochitl") {
    inXochitl = true;
    if (setupHooks() != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }

  auto* func_main =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
};
}
