#include "PreloadHooks.h"
#include <dlfcn.h>

PreloadHook&
PreloadHook::getInstance() {
  static PreloadHook instance;
  return instance;
}

// __has_feature is a clang-only extension - GCC (used for the ARM device
// build) errors on seeing it unexpanded, so it must stay inside its own
// defined(__has_feature) guard rather than in a compound #if.
#if defined(__has_feature)
#define RM2FB_HAS_ASAN __has_feature(address_sanitizer)
#else
#define RM2FB_HAS_ASAN 0
#endif

// Overriding malloc/calloc process-wide (below) fights AddressSanitizer for
// the same symbols and corrupts the process before __asan_init even runs -
// nothing in the test suite exercises these overrides, so skip them in ASan
// builds.
#if !RM2FB_HAS_ASAN

extern "C" {

// calloc is special, it's used by `dlsym` internally so we cannot call it in
// the hook. Instead we redirect to this internal symbol.
extern void*
__libc_calloc(size_t, size_t);

#define FUNC(id, res, name, ...)                                               \
  res name(__VA_ARGS__) {                                                      \
    static auto* originalFn = (res (*)(__VA_ARGS__))(                          \
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

#endif // !RM2FB_HAS_ASAN
