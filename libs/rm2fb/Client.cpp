#include "Client.h"

#include "IOCTL.h"
#include "ImageHook.h"

#include "Address.h"
#include "Socket.h"

#include "frida-gum.h"

#include <dlfcn.h>

#include <iostream>
#include <unistd.h>

namespace {

Socket clientSock;
bool inXochitl = false;

void
new_create_threads() {
  puts("HOOK: create threads called");
}

int
new_shutdown() {
  puts("HOOK: shutdown called");
  return 0;
}

int
setupHooks() {
  auto addrs = getAddresses();
  if (!addrs) {
    return EXIT_FAILURE;
  }

  gum_init_embedded();

  bool result = true;

  // Hook create Instance
  result = std::visit(
    [](auto createInstance) {
      return createInstance.hook((void*)new_create_threads);
    },
    addrs->createInstance);

  if (!result)
    return EXIT_FAILURE;

  // Hook update
  result = std::visit(
    [](auto update) { return update.hook((void*)sendUpdate); }, addrs->update);

  if (!result)
    return EXIT_FAILURE;

  // Hook shutdown
  result =
    std::visit([](auto shutdown) { return shutdown.hook((void*)new_shutdown); },
               addrs->shutdownThreads);

  if (!result)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

} // namespace

bool
sendUpdate(const UpdateParams& params) {
  clientSock.sendto(params);
  auto [result, _] = clientSock.recvfrom<bool>();
  return result.value_or(false);
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

  if (fb.mem == nullptr) {
    return EXIT_FAILURE;
  }

  if (!clientSock.bind(nullptr)) {
    return EXIT_FAILURE;
  }
  if (!clientSock.connect(DEFAULT_SOCK_ADDR)) {
    return EXIT_FAILURE;
  }

  char pathBuffer[PATH_MAX];
  readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer) == "/usr/bin/xochitl") {
    inXochitl = true;
    if (setupHooks() != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }

  auto* func_main =
    (typeof(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
};
}
