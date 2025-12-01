#include "AddressHooking.h"
#include "PreloadHooks.h"
#include "Version.h"
#include "rm2fb/SharedBuffer.h"

#include <dlfcn.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#include <rm2.h>

namespace {
constexpr auto usleep_delay = 1000;

bool* usleep_hook_var = nullptr; // NOLINT

struct ImageInfo {
  int32_t width;
  int32_t height;
  int32_t stride;
  int32_t zero1;
  int32_t zero2;
  uint16_t* data;
};

int
createThreadsHook(ImageInfo* info) {
  puts("HOOK: Create threads called!");
  const auto& fb = SharedFB::getInstance();
  info->width = fb_width;
  info->height = fb_height;
  info->stride = fb_width;
  info->zero1 = 0;
  info->zero2 = 0;
  info->data = static_cast<uint16_t*>(fb.getFb());
  return 0;
}

void
shutdownHook() {
  puts("HOOK: Shutdown called!");
}

void*
mallocHook(void* (*orig)(size_t), size_t size) {
  if (size == 0x503580) {
    std::cout << "HOOK: malloc redirected to shared FB\n";
    const auto& fb = SharedFB::getInstance();
    PreloadHook::getInstance().unhook<PreloadHook::Malloc>();
    return fb.getFb();
  }

  return orig(size);
}

int
usleepHook(int (*orig)(useconds_t), useconds_t micros) {
  if (usleep_hook_var != nullptr && micros == usleep_delay) {
    *usleep_hook_var = false;
    usleep_hook_var = nullptr;
    PreloadHook::getInstance().unhook<PreloadHook::USleep>();
  }
  return orig(micros);
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
///    * Hook malloc (called by createThreads), redirect to shared mem.
///    * Call createThreads (also creates mutexes)
///    * Call own 'do init' implementation
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

  pthread_mutex_t* updateMutex = nullptr;
  sem_t* updateSemaphore = nullptr;

  pthread_mutex_t* vsyncMutex = nullptr;
  pthread_cond_t* vsyncCond = nullptr;

  bool* globalInit = nullptr;

  AddressInfo(SimpleFunction createThreads,
              SimpleFunction update,
              SimpleFunction shutdownFn,
              pthread_mutex_t* updateMutex,
              sem_t* updateSem,
              pthread_mutex_t* vsyncMutex,
              pthread_cond_t* vsyncCond,
              bool* globalInit)
    : createThreads(createThreads)
    , update(update)
    , shutdownFn(shutdownFn)
    , updateMutex(updateMutex)
    , updateSemaphore(updateSem)
    , vsyncMutex(vsyncMutex)
    , vsyncCond(vsyncCond)
    , globalInit(globalInit) {}

  //=========================================================================//
  // Server
  //=========================================================================//

  void waitForInit() const {
    *globalInit = true;
    pthread_mutex_lock(vsyncMutex);
    pthread_cond_broadcast(vsyncCond);
    pthread_mutex_unlock(vsyncMutex);
    while (*globalInit) {
      usleep(usleep_delay);
    }
  }

  void initThreads() const final {
    PreloadHook::getInstance().hook<PreloadHook::Malloc>(mallocHook);

    ImageInfo info{};
    createThreads.call<void*, ImageInfo*>(&info);
    assert([&] {
      const auto& fb = SharedFB::getInstance();
      return info.data == fb.getFb();
    }() && "Malloc wasn't hooked?");
    waitForInit();
  }

  bool doUpdate(const UpdateParams& params) const final {
    UpdateParams mappedParams = params;
    mappedParams.waveform = mapNewWaveform(params.waveform);

    pthread_mutex_lock(updateMutex);
    update.call<void, const UpdateParams*>(&mappedParams);
    pthread_mutex_unlock(updateMutex);
    sem_post(updateSemaphore);

    return true;
  }
  void shutdownThreads() const final { shutdownFn.call<void>(); }

  //=========================================================================//
  // Client
  //=========================================================================//

  bool installHooks(UpdateFn* newUpdate) const final {
    puts("Hooking!");
    usleep_hook_var = globalInit;
    PreloadHook::getInstance().hook<PreloadHook::USleep>(usleepHook);

    createThreads.hook((void*)createThreadsHook);

    update.hook((void*)newUpdate);

    shutdownFn.hook((void*)shutdownHook);
    return true;
  }

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

const AddressInfo version_3_8_info = AddressInfo{
  SimpleFunction{ 0x4ab8d0 }, SimpleFunction{ 0x4a67c8 },
  SimpleFunction{ 0x4a5b7c }, (pthread_mutex_t*)0xa8498c,
  (sem_t*)0xa849a4,           (pthread_mutex_t*)0xa876e4,
  (pthread_cond_t*)0xa87700,  (bool*)0xa87734,
};

} // namespace

const AddressInfoBase* const version_3_8_2 = &version_3_8_info;
