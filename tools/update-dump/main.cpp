#include <dlfcn.h>
#include <iostream>
#include <stdlib.h>
#include <string_view>
#include <unistd.h>

#include "frida-gum.h"

namespace {

// temp is always 0.0?
// 3.6 values:
//   pen:          wave 1, flags 4, extra 1
//   highlighter:  wave 1, flags 4, extra 1
//
//   typing text:  wave 1, flags 4, extra 4
//   zoom/pan:     wave 6, flags 0, extra 4
//
//   b/w update:   wave 1, flags 0, extra 6
//   ui update:    wave 3, flags 0, extra 6
//
//   full refresh: wave 2, flags 1, extra 6
//
// suspicions:
//   - waveforms:
//     1: DU
//     2: GC16, high fidelity
//     3: GL16, lower fidelity
//     6: A2, fast pan & zoom
//   - extra == priority (pen highest, UI lowest, typing & pan in between)

gpointer originalUpdate = 0;
const void* update_address = (void*)0x4747e8;

struct UpdateParams2 {
  int x1;
  int y1;
  int x2;
  int y2;
  int flags;
  int waveform;
  float temp;
  int extra;
};

void
updateHook(UpdateParams2* params) {
  const auto& msg = *params;

  std::cerr << "Update  "
            << "{ { " << msg.x1 << ", " << msg.y1 << "; " << msg.x2 << ", "
            << msg.y2 << " }, wave: " << msg.waveform << " flags: " << msg.flags
            << " temp: " << msg.temp << " extra: " << msg.extra << " }\n";

  ((void (*)(UpdateParams2*))originalUpdate)(params);
}

int
setupHooks() {
  gum_init_embedded();

  auto* interceptor = gum_interceptor_obtain();

  auto result = gum_interceptor_replace(interceptor,
                                        (gpointer)update_address,
                                        (gpointer)updateHook,
                                        nullptr,
                                        &originalUpdate);

  return result == GUM_REPLACE_OK ? EXIT_SUCCESS : EXIT_FAILURE;
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

  char pathBuffer[PATH_MAX];
  readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer) == "/usr/bin/xochitl") {
    if (setupHooks() != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }

  auto* func_main =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
};
}
