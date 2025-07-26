#include "AddressHooking.h"
#include "SharedBuffer.h"
#include "Version.h"

#include <dlfcn.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#include <rm2.h>

namespace {

bool hook_first_malloc = false;       // NOLINT
bool hook_first_calloc = false;       // NOLINT
bool* sched_yield_hook_var = nullptr; // NOLINT

struct ImageInfo {
  int32_t width;
  int32_t height;
  int32_t stride;
  int32_t zero1;
  int32_t zero2;
  uint16_t* data;
  char* backBuffer;
};

int
createThreadsHook(ImageInfo* info) {
  puts("HOOK: Create threads called!");
  const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
  info->width = fb_width;
  info->height = fb_height;
  info->stride = fb_width;
  info->zero1 = 0;
  info->zero2 = 0;
  info->data = static_cast<uint16_t*>(fb.mem.get());
  info->backBuffer = (char*)fb.getGrayBuffer();
  return 0;
}

void
shutdownHook() {
  puts("HOOK: Shutdown called!");
}

/// TODO:
///  * Get client to work, hooking addresses like before
///  * Get server to actually work.
///    - [x] Init by calling EPFramebufferSwtcon::initialize
///    - [ ] Udpate by calling lockMutex, update, unlock & post.
///
///

void*
getQsgepaperHandle() {
  static auto* handle = [] {
    auto* handle =
      dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_LAZY);
    if (handle == nullptr) {
      std::cerr << dlerror() << "\n";
      exit(EXIT_FAILURE);
    }
    return handle;
  }();
  return handle;
}

/// client:
///  * Hook createThreads, returning shared mem in 'ImageInfo'
///  * Hook usleep, set 'do init' flag on call
///  --> We can call 'createInstance' without blocking
///
///  * Hook doUpdate, redirect to socket
///  * Hook shutdown, destroy created update mutex
///
/// server:
///  * initThreads
///    * dlopen(libqsgepaper)
///    * Hook malloc (called by createThreads), redirect to shared mem.
///    * Call EPFramebufferSwtcon::initialize()
///  * doUpdate
///    * Add extra update params
///    * Lock update mutex
///    * Call update function
///    * Unlock update mutex
///    * Post semaphore
///  * shutdownThreads, just call
///
struct AddressInfo : public AddressInfoBase {
  SimpleFunction createThreads{};

  SimpleFunction update{};
  SimpleFunction shutdownFn{};

  bool* globalInit = nullptr;
  bool* hasShutdown = nullptr;

  AddressInfo(SimpleFunction createThreads,
              SimpleFunction update,
              SimpleFunction shutdownFn,
              bool* globalInit,
              bool* hasShutdown)
    : createThreads(createThreads)
    , update(update)
    , shutdownFn(shutdownFn)
    , globalInit(globalInit)
    , hasShutdown(hasShutdown) {}

  //=========================================================================//
  // Server
  //=========================================================================//

  void initThreads() const final {
    auto* initialize =
      dlsym(getQsgepaperHandle(), "_ZN19EPFramebufferSwtcon10initializeEv");
    if (initialize == nullptr) {
      std::cerr << dlerror() << "\n";
      exit(EXIT_FAILURE);
    }

    printf("Initialize: 0x%p\n", initialize);
    auto createThreads = SimpleFunction{ (uintptr_t)initialize };

    /// Initialize calls the Qimage copy constructor on these, so pass some
    /// dummy buffer.
    auto* fakeQimage = malloc(2 * 0xc);
    hook_first_malloc = true;
    hook_first_calloc = true;
    createThreads.call<void*, void*>(fakeQimage);
    // TODO: verify qiamge data?
  }

  bool doUpdate(const UpdateParams& params) const final {
    static auto updateFn = [] {
      auto* updateFnPtr =
        dlsym(getQsgepaperHandle(),
              "_ZN19EPFramebufferSwtcon6updateE5QRecti9PixelModei");
      if (updateFnPtr == nullptr) {
        std::cerr << dlerror() << "\n";
        exit(EXIT_FAILURE);
      }
      printf("Update addr: 0x%p\n", updateFnPtr);
      return SimpleFunction{ (uintptr_t)updateFnPtr - 0x38b58 + 0x3ccdc };
    }();
    auto lockFn = SimpleFunction{ updateFn.address - 0x3ccdc + 0x3b6d8 };
    auto unlockFn = SimpleFunction{ updateFn.address - 0x3ccdc + 0x3ddc0 };

    UpdateParams mappedParams = params;
    mappedParams.waveform = mapNewWaveform(params.waveform);

    lockFn.call<void>();
    updateFn.call<void, const UpdateParams*>(&mappedParams);
    unlockFn.call<void>();

    return true;
  }

  void shutdownThreads() const final {
    static auto shutdownFn = [] {
      auto* shutdownFn =
        dlsym(getQsgepaperHandle(), "_ZN19EPFramebufferSwtcon8shutdownEv");
      if (shutdownFn == nullptr) {
        std::cerr << dlerror() << "\n";
        exit(EXIT_FAILURE);
      }
      return SimpleFunction{ (uintptr_t)shutdownFn };
    }();
    shutdownFn.call<void>();
  }

  //=========================================================================//
  // Client
  //=========================================================================//

  bool installHooks(UpdateFn* newUpdate) const final {
    puts("Hooking!");
    sched_yield_hook_var = globalInit;

    createThreads.hook((void*)createThreadsHook);

    update.hook((void*)newUpdate);

    shutdownFn.hook((void*)shutdownHook);

    // TODO: hook usleep instead?
    *hasShutdown = true;

    return true;
  }

  /// Xochitl 3.20 waveform & flag use:
  ///
  ///         | wave | flags | extra2
  /// --------+------+-------+-------
  /// refresh | 2    | 1     | 9
  /// UI      | 3    | 0     | 9
  /// UI      | 3    | 0     | 6 (sometimes, not sure when)
  /// Progres | 1    | 0     | 6  (fast UI?)
  /// Pen     | 1    | 2     | 7
  /// Marker  | 1    | 2     | 7 -- Broken? Needs extra2!
  /// Pan     | 6    | 0     | 6 (with 3|0 for scroll UI)
  /// key/UI  | 6    | 0     | 6
  /// Shape   | 6    | 0     | 6
  ///
  static int mapNewWaveform(int waveform) {
    if ((waveform & UpdateParams::ioctl_waveform_flag) == 0) {
      return waveform;
    }

    waveform &= ~UpdateParams::ioctl_waveform_flag;
    switch (waveform) {
      case WAVEFORM_MODE_INIT:
        return 2;
      case WAVEFORM_MODE_DU:
      default:
        return 1;
      case WAVEFORM_MODE_GC16:
        return 2;
      case WAVEFORM_MODE_GL16:
        return 3;
      case WAVEFORM_MODE_A2:
        return 6;
    }
  }
};

const AddressInfo version_3_20_info = AddressInfo{
  SimpleFunction{ 0x73a3c0 }, SimpleFunction{ 0x7919c8 },
  SimpleFunction{ 0x739c80 }, (bool*)0x11ba2c0,
  (bool*)0x11b7478,
};

} // namespace

const AddressInfoBase* const version_3_20_0 = &version_3_20_info;

extern "C" {

int
sched_yield() {
  if (sched_yield_hook_var != nullptr) {
    *sched_yield_hook_var = false;
    sched_yield_hook_var = nullptr;
  }

  static auto originalFn = (int (*)())dlsym(RTLD_NEXT, "sched_yield");
  return originalFn();
}

void*
malloc(size_t size) {
  if (size == 0x503580 && hook_first_malloc) {
    std::cout << "HOOK: malloc redirected to shared FB\n";
    const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
    hook_first_malloc = false;
    return fb.mem.get();
  }

  static auto funcMalloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
  return funcMalloc(size);
}

void*
calloc(size_t size, size_t count) {
  if (size == 0x281ac0 && count == 1 && hook_first_calloc) {
    std::cout << "HOOK: calloc redirected to shared FB\n";
    const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
    hook_first_calloc = false;
    return fb.getGrayBuffer();
  }

  static auto funcCalloc =
    (void* (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
  return funcCalloc(size, count);
}
}
