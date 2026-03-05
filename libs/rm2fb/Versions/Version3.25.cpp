#include "AddressHooking.h"
#include "PreloadHooks.h"
#include "SharedBuffer.h"
#include "Version.h"

#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <pthread.h>

#include <rm2.h>

/// Firmware 3.25 support for rM2-stuff.
///
/// Key architectural change from 3.20-3.23:
///   EPFramebufferSwtcon is statically linked into xochitl (no longer in
///   libqsgepaper.so). The rm2fb server MUST run in LD_PRELOAD mode
///   (ServerLib.cpp path) where it hijacks xochitl's __libc_start_main.
///
/// Strategy:
///   Redirect the grayscale buffer allocation to shared memory so the
///   internal update function reads client pixel data when building
///   UpdateMsgs. The update sequence (from Fusion vtable[23]):
///     1. pthread_mutex_lock(swtconData + 0x54)   — lock the queue mutex
///     2. actualUpdate(0x57e6f0) — creates UpdateMsgs at swtconData+0x58BC
///     3. processAndSignal(0x57d5e4) — processes msgs from the queue,
///        unlocks the mutex, atomically increments the futex word at
///        swtconData+0x30, and calls futex_wake to wake the framegen thread.

namespace {

// /dev/fb0 mmap size for firmware 3.25.1.1
constexpr size_t fb0_mmap_size = 0x17BD800; // 24,893,440 bytes

// Real /dev/fb0 mmap pointer, captured by mmapHook.
static void* realFbMmap = nullptr; // NOLINT

// Track which buffer redirections succeeded.
static bool redirectedGray = false; // NOLINT
static void* pixelBuffer32 = nullptr; // NOLINT

// Count large mallocs so we can unhook after init is done.
static int largeMallocCount = 0; // NOLINT
// Count 0x281ac0 (gray buffer) allocations.
static int grayAllocCount = 0; // NOLINT

// Qt function pointers resolved via dlsym during init.
// QRegion(int x, int y, int w, int h, RegionType t) — RegionType::Rectangle = 0.
using QRegionCtorFn = void (*)(void*, int, int, int, int, int);
using QRegionDtorFn = void (*)(void*);
static QRegionCtorFn qregionCtor = nullptr; // NOLINT
static QRegionDtorFn qregionDtor = nullptr; // NOLINT

// The actual update function inside xochitl (creates UpdateMsgs).
// Signature: void update(void* this, QRegion& region, int waveform, int mode, int flags)
// 'this' is ignored (function uses global addresses for all swtcon state).
using ActualUpdateFn = void (*)(void*, void*, int, int, int);
static constexpr uintptr_t actualUpdateAddr = 0x57e6f0;

// processAndSignal: processes UpdateMsgs from the queue at swtconData+0x58BC,
// resets the counter at +0x58C4, unlocks the mutex at +0x54, atomically
// increments the futex word at +0x30, and wakes the framegen thread.
// Takes no explicit args — accesses swtconData via the global at 0x013240E4.
using ProcessAndSignalFn = void (*)();
static constexpr uintptr_t processAndSignalAddr = 0x57d5e4;

// swtconData global pointer location.
static constexpr uintptr_t swtconDataPtrAddr = 0x013240E4;
// Offset of the queue mutex within swtconData.
static constexpr uintptr_t queueMutexOffset = 0x54;

void*
mmapHook(void* (*orig)(void*, size_t, int, int, int, off_t),
         void* addr,
         size_t length,
         int prot,
         int flags,
         int fd,
         off_t offset) {
  if (length == fb0_mmap_size) {
    realFbMmap = orig(addr, length, prot, flags, fd, offset);
    PreloadHook::getInstance().unhook<PreloadHook::Mmap>();
    return realFbMmap;
  }
  return orig(addr, length, prot, flags, fd, offset);
}

void*
mallocHook(void* (*orig)(size_t), size_t size) {
  if (size >= 0x10000) {
    largeMallocCount++;
  }

  // 8-bit grayscale buffer (1404×1872×1 = 0x281ac0).
  // Redirect to shared memory so the update function reads client pixels.
  // There may be TWO gray allocations (current + prev). Redirect the first.
  if (size == 0x281ac0) {
    auto* ptr = orig(size);
    grayAllocCount++;
    if (!redirectedGray) {
      const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
      redirectedGray = true;
      return fb.getGrayBuffer();
    }
    return ptr;
  }

  // 32-bit ARGB buffer (1404×1872×4 = 0xa06b00).
  // Capture pointer — the update function also reads from this.
  if (size == 0xa06b00 && pixelBuffer32 == nullptr) {
    auto* ptr = orig(size);
    pixelBuffer32 = ptr;
    return ptr;
  }

  if (largeMallocCount >= 30) {
    PreloadHook::getInstance().unhook<PreloadHook::Malloc>();
  }

  return orig(size);
}

void*
callocHook(void* (*orig)(size_t, size_t), size_t size, size_t count) {
  if (size == 0x281ac0 && count == 1 && !redirectedGray) {
    const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
    redirectedGray = true;
    PreloadHook::getInstance().unhook<PreloadHook::Calloc>();
    return fb.getGrayBuffer();
  }
  if (size == 1 && count == 0x281ac0 && !redirectedGray) {
    const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
    redirectedGray = true;
    PreloadHook::getInstance().unhook<PreloadHook::Calloc>();
    return fb.getGrayBuffer();
  }
  return orig(size, count);
}

struct AddressInfo : public AddressInfoBase {
  struct Addresses {
    uintptr_t funcInit;           // EPFramebuffer::instance()
    uintptr_t funcActualUpdate;   // The real update function (creates UpdateMsgs)
    uintptr_t funcShutdown;       // shutdown handler
    uintptr_t* singletonPtr;     // &EPFramebuffer singleton
    bool* hasShutdownFlag;
  };
  Addresses addrs;

  AddressInfo(Addresses addrs) : addrs(addrs) {}

  //=========================================================================//
  // Server
  //=========================================================================//

  void initThreads() const final {
    PreloadHook::getInstance().hook<PreloadHook::Mmap>(mmapHook);
    PreloadHook::getInstance().hook<PreloadHook::Malloc>(mallocHook);
    PreloadHook::getInstance().hook<PreloadHook::Calloc>(callocHook);

    // Resolve QRegion constructor/destructor from Qt.
    qregionCtor = reinterpret_cast<QRegionCtorFn>(
      dlsym(RTLD_DEFAULT, "_ZN7QRegionC1EiiiiNS_10RegionTypeE"));
    if (qregionCtor == nullptr) {
      qregionCtor = reinterpret_cast<QRegionCtorFn>(
        dlsym(RTLD_DEFAULT, "_ZN7QRegionC2EiiiiNS_10RegionTypeE"));
    }
    qregionDtor = reinterpret_cast<QRegionDtorFn>(
      dlsym(RTLD_DEFAULT, "_ZN7QRegionD1Ev"));
    if (qregionDtor == nullptr) {
      qregionDtor = reinterpret_cast<QRegionDtorFn>(
        dlsym(RTLD_DEFAULT, "_ZN7QRegionD2Ev"));
    }

    // Create QCoreApplication — required by EPFramebuffer::instance().
    {
      using QCoreAppCtor = void (*)(void*, int&, char**, int);
      auto* ctor = reinterpret_cast<QCoreAppCtor>(
        dlsym(RTLD_DEFAULT, "_ZN16QCoreApplicationC1ERiPPci"));
      if (ctor == nullptr) {
        ctor = reinterpret_cast<QCoreAppCtor>(
          dlsym(RTLD_DEFAULT, "_ZN16QCoreApplicationC1ERiPPc"));
      }
      if (ctor != nullptr) {
        static char qcaBuffer[512] = {};
        static int fakeArgc = 1;
        static const char* fakeArgv[] = { "rm2fb_server", nullptr };
        ctor(qcaBuffer, fakeArgc, const_cast<char**>(fakeArgv), 0);
      }
    }

    auto* result = SimpleFunction{ addrs.funcInit }.call<void*>();

    printf("3.25 init: instance=%p fb0=%p gray=%s argb=%p qregion=%s\n",
           result, realFbMmap,
           redirectedGray ? "redirected" : "FAILED",
           pixelBuffer32,
           qregionCtor ? "found" : "MISSING");

    if (!redirectedGray) {
      std::cerr << "WARNING: gray buffer was not redirected!\n";
    }

    // Ensure hooks are unhooked.
    if (PreloadHook::getInstance().isHooked<PreloadHook::Malloc>()) {
      PreloadHook::getInstance().unhook<PreloadHook::Malloc>();
    }
    if (PreloadHook::getInstance().isHooked<PreloadHook::Calloc>()) {
      PreloadHook::getInstance().unhook<PreloadHook::Calloc>();
    }
  }

  bool doUpdate(const UpdateParams& params) const final {
    void* instance = reinterpret_cast<void*>(*addrs.singletonPtr);
    if (instance == nullptr) {
      return false;
    }

    if (qregionCtor == nullptr || qregionDtor == nullptr) {
      return false;
    }

    // Get swtconData.
    auto* swtconDataPtr = reinterpret_cast<void**>(swtconDataPtrAddr);
    void* swtconData = *swtconDataPtr;
    if (swtconData == nullptr) {
      return false;
    }

    const auto& fb = unistdpp::fatalOnError(SharedFB::getInstance());
    const auto* src = static_cast<const uint16_t*>(fb.mem.get());

    // Clamp dirty rect to framebuffer bounds.
    int x1 = params.x1, y1 = params.y1, x2 = params.x2, y2 = params.y2;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= fb_width) x2 = fb_width - 1;
    if (y2 >= fb_height) y2 = fb_height - 1;
    if (x1 > x2 || y1 > y2) {
      // Empty or invalid rect — convert everything as fallback.
      x1 = 0; y1 = 0; x2 = fb_width - 1; y2 = fb_height - 1;
    }

    // Convert only the dirty rect from RGB565 → Y8 gray + ARGB32.
    if (redirectedGray || pixelBuffer32 != nullptr) {
      auto* grayDst = redirectedGray
        ? static_cast<uint8_t*>(fb.getGrayBuffer()) : nullptr;
      auto* argbDst = pixelBuffer32 != nullptr
        ? static_cast<uint32_t*>(pixelBuffer32) : nullptr;

      for (int y = y1; y <= y2; y++) {
        int rowStart = y * fb_width + x1;
        int rowEnd = y * fb_width + x2;
        for (int i = rowStart; i <= rowEnd; i++) {
          uint16_t px = src[i];
          uint32_t r = ((px >> 11) & 0x1F) * 255 / 31;
          uint32_t g = ((px >> 5) & 0x3F) * 255 / 63;
          uint32_t b = (px & 0x1F) * 255 / 31;
          if (grayDst != nullptr)
            grayDst[i] = static_cast<uint8_t>((r * 77 + g * 150 + b * 29) >> 8);
          if (argbDst != nullptr)
            argbDst[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
      }
    }

    auto* sd = static_cast<char*>(swtconData);

    // Construct a QRegion for the dirty rectangle.
    alignas(8) char regionBuf[16] = {};
    int rx = 0, ry = 0, rw = fb_width, rh = fb_height;
    if (params.x2 > params.x1 && params.y2 > params.y1) {
      rx = params.x1;
      ry = params.y1;
      rw = params.x2 - params.x1;
      rh = params.y2 - params.y1;
    }
    qregionCtor(regionBuf, rx, ry, rw, rh, 0 /* Rectangle */);

    // Lock the queue mutex, create UpdateMsgs, process and wake framegen.
    // Only mode=7 ("changed region") — skipping mode=12 ("unchanged") avoids
    // the complement region overriding the dirty region.
    auto* queueMutex = reinterpret_cast<pthread_mutex_t*>(sd + queueMutexOffset);
    pthread_mutex_lock(queueMutex);

    auto* updateFn = reinterpret_cast<ActualUpdateFn>(addrs.funcActualUpdate);
    updateFn(instance, regionBuf, 2, 7, 1);

    auto* processAndSignal = reinterpret_cast<ProcessAndSignalFn>(processAndSignalAddr);
    processAndSignal();

    qregionDtor(regionBuf);

    return true;
  }

  void shutdownThreads() const final {
    SimpleFunction{ addrs.funcShutdown }.call<void>();
  }

  //=========================================================================//
  // Client
  //=========================================================================//

  bool installHooks(UpdateFn* newUpdate) const final {
    puts("Hooking for 3.25 (client apps use open/ioctl hooks)!");
    return true;
  }

  /// Waveform mapping for 3.25 (same Swtcon hardware as 3.20).
  static UpdateParams mapUpdate(const UpdateParams& update) {
    UpdateParams res = update;
    if ((update.waveform & UpdateParams::ioctl_waveform_flag) == 0) {
      return res;
    }

    res.waveform &= ~UpdateParams::ioctl_waveform_flag;
    res.waveform = [&] {
      switch (res.waveform) {
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
    }();

    if ((update.flags & 4) != 0) {
      res.flags = 2;
      res.extraMode = 6;
    } else if ((update.flags & 0x1) == 0) {
      res.flags = 0;
      res.extraMode = 6;
    } else {
      res.flags = 1;
      res.extraMode = 9;
    }

    return res;
  }
};

const AddressInfo version_3_25_info = AddressInfo::Addresses{
  .funcInit = 0x583020,
  .funcActualUpdate = actualUpdateAddr,
  .funcShutdown = 0x7438F8,
  .singletonPtr = (uintptr_t*)0x01324090,
  .hasShutdownFlag = (bool*)0x0132AD00,
};

} // namespace

const AddressInfoBase* const version_3_25_0 = &version_3_25_info;
