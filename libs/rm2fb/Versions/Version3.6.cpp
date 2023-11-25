#include "../ImageHook.h"
#include "AddressHooking.h"
#include "Version.h"

#include <dlfcn.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#include <rm2.h>

namespace {
constexpr auto usleep_delay = 1000;

bool hook_first_malloc = false;  // NOLINT
bool* usleep_hook_var = nullptr; // NOLINT

AddressInfoBase::UpdateFn* global_new_update = nullptr; // NOLINT

struct ImageInfo {
  int32_t width;
  int32_t height;
  int32_t stride;
  int32_t zero1;
  int32_t zero2;
  uint16_t* data;
};

struct ExtendedUpdateParams {
  UpdateParams params;
  float extra1;
  int extra2;
};
static_assert(sizeof(ExtendedUpdateParams) == 0x20);

int
createThreadsHook(ImageInfo* info) {
  puts("HOOK: Create threads called!");
  info->width = fb_width;
  info->height = fb_height;
  info->stride = fb_width;
  info->zero1 = 0;
  info->zero2 = 0;
  info->data = fb.mem;
  return 0;
}

void
updateHook(const ExtendedUpdateParams* params) {
  global_new_update(params->params);
}

void
shutdownHook() {
  puts("HOOK: Shutdown called!");
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
    hook_first_malloc = true;
    ImageInfo info{};
    createThreads.call<void*, ImageInfo*>(&info);
    assert(info.data == fb.mem && "Malloc wasn't hooked?");
    waitForInit();
  }

  bool doUpdate(const UpdateParams& params) const final {
    ExtendedUpdateParams extraParams{
      .params = params,
      .extra1 = 0.0F,
      .extra2 = 0, // TODO: set priority?
    };
    extraParams.params.waveform = mapNewWaveform(params.waveform);

    pthread_mutex_lock(updateMutex);
    update.call<void, const ExtendedUpdateParams*>(&extraParams);
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

    createThreads.hook((void*)createThreadsHook);

    global_new_update = newUpdate;
    update.hook((void*)updateHook);

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

extern "C" {

int
usleep(useconds_t micros) {
  if (usleep_hook_var != nullptr && micros == usleep_delay) {
    *usleep_hook_var = false;
    usleep_hook_var = nullptr;
  }
  static auto funcUSleep = (int (*)(useconds_t))dlsym(RTLD_NEXT, "usleep");
  return funcUSleep(micros);
}

void*
malloc(size_t size) {
  if (size == 0x503580 && hook_first_malloc) {
    hook_first_malloc = false;
    return fb.mem;
  }

  static auto funcMalloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
  return funcMalloc(size);
}
}
