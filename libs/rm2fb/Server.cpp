#include "Address.h"
#include "ImageHook.h"
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
  std::visit([](auto createInstance) { createInstance.template call<void*>(); },
             addrs->createInstance);

  std::cout << "SWTCON initalized!\n";

  if (strcmp(argv[0], "/usr/bin/xochitl") == 0) {
    strcpy(argv[0], "__rm2fb_server__");
  }

  while (running) {
    auto [msg, addr] = serverSock.recvfrom<UpdateParams>();
    if (!msg.has_value()) {
      continue;
    }

    bool res = std::visit(
      [msg = msg](auto update) {
        return update.template call<bool, const UpdateParams*>(&*msg);
      },
      addrs->update);
    // ptrs->update(*msg, nullptr, 0);

    // Don't log Stroke updates
    if (msg->flags != 4) {
      std::cerr << "UPDATE " << *msg << ": " << res << "\n";
    }

    serverSock.sendto(res, addr);
  }

  std::visit([](auto shutdown) { shutdown.template call<void>(); },
             addrs->shutdownThreads);
  // ptrs->shutdownThreads();

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
    (typeof(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(server_main, argc, argv, init, fini, rtld_fini, stack_end);
};
}
