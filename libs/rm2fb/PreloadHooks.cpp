#include "PreloadHooks.h"
#include <dlfcn.h>

PreloadHook&
PreloadHook::getInstance() {
  static PreloadHook instance;
  return instance;
}

extern "C" {
#define FUNC(id, res, name, ...)                                               \
  res name(__VA_ARGS__) {                                                      \
    static auto* originalFn = (res(*)(__VA_ARGS__))dlsym(RTLD_NEXT, #name);    \
    if (auto* ptr = PreloadHook::getInstance().getHook<PreloadHook::id>();     \
        ptr != nullptr) {                                                      \
      return ptr(originalFn, __VA_ARGS__);                                     \
    }                                                                          \
    return originalFn(__VA_ARGS__);                                            \
  }

// NOLINTNEXTLINE(readability-identifier-naming)
HOOKS(FUNC)
}
