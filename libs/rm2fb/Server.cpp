#include "Address.h"
#include "ImageHook.h"
#include "Message.h"
#include "Socket.h"

#include <atomic>
#include <dlfcn.h>
#include <iostream>
#include <signal.h>
#include <string.h>
#include <unistd.h>

namespace {

std::atomic_bool running = true;

void
onSigint(int num) {
  running = false;
}

void
setupExitHandler() {
  struct sigaction action;

  action.sa_handler = onSigint;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("Sigaction");
    exit(EXIT_FAILURE);
  }
}

int
server_main(int argc, char* argv[], char** envp) {
  setupExitHandler();

  const bool debugMode = [] {
    const auto* debugEnv = getenv("RM2FB_DEBUG");
    return debugEnv != nullptr && debugEnv != std::string_view("0");
  }();
  if (debugMode) {
    std::cerr << "Debug Mode!\n";
  }

  const char* socketAddr = /*argc == 2 ? argv[1] :*/ DEFAULT_SOCK_ADDR;

  // Setup server socket.
  if (unlink(socketAddr) != 0) {
    perror("Socket unlink");
  }

  Socket serverSock;
  if (!serverSock.bind(socketAddr)) {
    return EXIT_FAILURE;
  }

  // Get addresses
  auto addrs = getAddresses();
  if (!addrs) {
    std::cerr << "Failed to get addresses\n";
    return EXIT_FAILURE;
  }

  // Open shared memory
  if (fb.mem == nullptr) {
    return EXIT_FAILURE;
  }
  memset(fb.mem, 0xff, fb_size);

  // Call the get or create Instance function.
  addrs->initThreads();

  std::cout << "SWTCON initalized!\n";

  if (strcmp(argv[0], "/usr/bin/xochitl") == 0) {
    strcpy(argv[0], "__rm2fb_server__");
  }

  while (running) {
    auto [msg, addr] = serverSock.recvfrom<UpdateParams>();
    if (!msg.has_value()) {
      continue;
    }

    bool res = addrs->doUpdate(*msg);

    // Don't log Stroke updates
    if (debugMode || msg->flags != 4) {
      std::cerr << "UPDATE " << *msg << ": " << res << "\n";
    }

    serverSock.sendto(res, addr);
  }

  return EXIT_SUCCESS;
}

} // namespace

extern "C" {

int
__libc_start_main(int (*_main)(int, char**, char**),
                  int argc,
                  char** argv,
                  int (*init)(int, char**, char**),
                  void (*fini)(void),
                  void (*rtld_fini)(void),
                  void* stack_end) {

  std::cout << "STARTING RM2FB\n";

  auto* func_main =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(server_main, argc, argv, init, fini, rtld_fini, stack_end);
};
}
