#include "PreloadHooks.h"
#include <dlfcn.h>

PreloadHook&
PreloadHook::getInstance() {
  static PreloadHook instance;
  return instance;
}

extern "C" {

// calloc is special, it's used by `dlsym` internally so we cannot call it in
// the hook. Instead we redirect to this internal symbol.
extern void*
__libc_calloc(size_t, size_t);

#define FUNC(id, res, name, ...)                                               \
  res name(__VA_ARGS__) {                                                      \
    static auto* originalFn = (res(*)(__VA_ARGS__))(                           \
      PreloadHook::id == PreloadHook::Calloc ? (void*)__libc_calloc            \
                                             : dlsym(RTLD_NEXT, #name));       \
    if (auto* ptr = PreloadHook::getInstance().getHook<PreloadHook::id>();     \
        ptr != nullptr) {                                                      \
      return ptr(originalFn, __VA_ARGS__);                                     \
    }                                                                          \
    return originalFn(__VA_ARGS__);                                            \
  }

// NOLINTNEXTLINE(readability-identifier-naming)
HOOKS(FUNC)
}
