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
  if (!sendUpdate(UpdateParams{
        .y1 = 0,
        .x1 = 0,
        .y2 = 0,
        .x2 = 0,
        .flags = 0,
        .waveform = 0,
        .temperatureOverride = 0,
        .extraMode = 0,
      })) {
    std::cerr << "Init failed, no server running\n";
    std::exit(EXIT_FAILURE);
  }
}

} // namespace

bool
sendUpdate(const UpdateParams& params) {
  auto& clientSock = getControlSocket();
  if (!clientSock.isValid()) {
    return false;
  }

  return clientSock.writeAll(params)
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

  // Don't kill ourselves when SIGPIPE happens because rm2fb went down.
  // It might come back up later!
  std::signal(SIGPIPE, SIG_IGN);

  char pathBuffer[PATH_MAX];
  auto size = readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer, size) == "/usr/bin/xochitl") {
    inXochitl = true;

    unistdpp::fatalOnError(SharedFB::getInstance(), "Error making shared FB");

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
