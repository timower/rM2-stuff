#include "Server.h"

#include <iostream>
#include <dlfcn.h>

extern "C" {

int
__libc_start_main(int (*_main)(int, char**, char**), // NOLINT
                  int argc,
                  char** argv,
                  int (*init)(int, char**, char**),
                  void (*fini)(),
                  void (*rtldFini)(),
                  void* stackEnd) {

  std::cout << "STARTING RM2FB\n";

  // NOLINTNEXTLINE
  auto* funcMain = reinterpret_cast<decltype(&__libc_start_main)>(
    dlsym(RTLD_NEXT, "__libc_start_main"));

  return funcMain(serverMain, argc, argv, init, fini, rtldFini, stackEnd);
};
}
