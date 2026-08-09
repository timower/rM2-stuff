#include "swtcon.h"
#include "swtcon_libimpl.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <linux/fb.h>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "display.h"
#include "init.h"
#include "qsgepaper_globals.h"

// SWTCON_LIBIMPL mode dlopen's the real libqsgepaper.so and calls its
// by-address exports directly - only meaningful against the real armv7
// binary (see swtcon_libimpl.h's comment), so on any other host dlopen
// would just fail architecture-mismatch. Compiled out entirely there
// (rather than left runtime-dead behind swtcon_lib_impl_enabled()'s env-var
// check) so coverage tooling doesn't count this unreachable-on-dev-host code
// as missing coverage.
#if SWTCON_32BIT_ABI_CHECK

// Symbol addresses for version 3.23.0.54 / 3.23.0.64 (assumes 0x10000 image
// base)
#define INSTANCE_ADDR 0x35de0
#define INIT_ADDR 0x3b814
#define UPDATE_ADDR 0x3ccac
#define LOCK_ADDR 0x3b690
#define UNLOCK_POST_ADDR 0x3dd90
#define WAIT_ADDR 0x3b644
#define FLASH_ADDR 0x3b4b4
#define SHUTDOWN_ADDR 0x3b6b4

typedef int (*init_func_t)(void* state_out, int zero);

static void* libqsgepaper_handle = nullptr;
static uintptr_t runtime_offset = 0;

static init_func_t qsgepaper_init = nullptr;

// Library exports for A/B comparison against the native update
// reimplementation - see update.cpp's NATIVE_UPDATE switch. Non-static
// (extern) so update.cpp can reach them for its `#else` fallback.
void (*qsgepaper_lock)() = nullptr;
void (*qsgepaper_update)(update_data*) = nullptr;
void (*qsgepaper_unlock_post)() = nullptr;
void (*qsgepaper_wait)() = nullptr;

// EPFramebufferSwtcon::initialize (0x38e30) calls qsgepaper_init and then
// this - the startup flash, blocking until it completes - before doing
// anything else (see request_flash_and_wait's comment, which mirrors
// this natively). Library mode needs to call it explicitly too: unlike the
// other four, it isn't reached through any of the five public swtcon_*
// entry points on its own.
static void (*qsgepaper_flash)() = nullptr;

// actualShutdown (0x3b6b4) - library mode calls this by address too, the
// same way it calls init/lock/update/unlock_post/wait/flash, instead of
// re-running the native shutdown reimplementation against the library's own
// UpdateQueueGlobals via resolve_ptr. Confirmed via Ghidra decompile to be
// exactly the sequence swtcon_shutdown's native branch below already
// mirrors (drain queues, signal+join both threads, save statebuffer,
// blank+free framebuffer/statebuffer/LUT, unlock pid file).
static void (*qsgepaper_shutdown)(int) = nullptr;

static void
load_lib() {
  if (libqsgepaper_handle)
    return;

  libqsgepaper_handle = dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so",
                               RTLD_NOW | RTLD_GLOBAL);
  if (!libqsgepaper_handle) {
    libqsgepaper_handle = dlopen("./libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
    if (!libqsgepaper_handle) {
      std::cerr << "Failed to load libqsgepaper.so: " << dlerror() << std::endl;
      exit(1);
    }
  }

  void* instance_func =
    dlsym(libqsgepaper_handle, "_ZN13EPFramebuffer8instanceEv");
  if (!instance_func) {
    std::cerr << "Failed to find known export symbol: " << dlerror()
              << std::endl;
    exit(1);
  }

  runtime_offset = (uintptr_t)instance_func - INSTANCE_ADDR;

  qsgepaper_init = (init_func_t)(runtime_offset + INIT_ADDR);
  qsgepaper_lock = (void (*)())(runtime_offset + LOCK_ADDR);
  qsgepaper_update = (void (*)(update_data*))(runtime_offset + UPDATE_ADDR);
  qsgepaper_unlock_post = (void (*)())(runtime_offset + UNLOCK_POST_ADDR);
  qsgepaper_wait = (void (*)())(runtime_offset + WAIT_ADDR);
  qsgepaper_flash = (void (*)())(runtime_offset + FLASH_ADDR);
  qsgepaper_shutdown = (void (*)(int))(runtime_offset + SHUTDOWN_ADDR);
}

uintptr_t
swtcon_runtime_offset() {
  return runtime_offset;
}

bool
swtcon_lib_impl_enabled() {
  static const bool enabled = getenv("SWTCON_LIBIMPL") != nullptr;
  return enabled;
}

#else // !SWTCON_32BIT_ABI_CHECK

uintptr_t
swtcon_runtime_offset() {
  return 0;
}

#endif // SWTCON_32BIT_ABI_CHECK

uint16_t*
swtcon_init(const InitParams& params) {
  std::cout << "swtcon_init: initialization sequence starting..." << std::endl;

#if SWTCON_32BIT_ABI_CHECK
  if (swtcon_lib_impl_enabled()) {
    // Library mode always drives real hardware - InitParams' native-mode
    // injection seams don't apply here.
    load_lib();

    char state[256];
    memset(state, 0, sizeof(state));

    std::cout << "Initializing swtcon (library mode)..." << std::endl;
    int init_res = qsgepaper_init(state, 0);
    if (init_res != 0) {
      std::cerr << "Error initializing swtcon, res: " << init_res << std::endl;
      return nullptr;
    }

    // Matches EPFramebufferSwtcon::initialize's own call sequence (see
    // qsgepaper_flash's comment) - native mode does the equivalent via
    // request_flash_and_wait() at the end of its own init path below.
    std::cout << "Requesting startup flash (library mode)..." << std::endl;
    qsgepaper_flash();

    uint16_t* image = *(uint16_t**)(state + 0x14);
    return image;
  }
#endif // SWTCON_32BIT_ABI_CHECK

  // --- NATIVE INIT IMPLEMENTATION ---
  // Reset all module-level state first - swtcon_shutdown doesn't zero scalar
  // bookkeeping, so a second swtcon_init() in one process would otherwise
  // see stale state (e.g. workerThreadShutdown still set). Must happen
  // before create_pid_file/init_statebuffer below, which now write directly
  // into this same state rather than into their own separate globals.
  swtcon_state()->reset();

  auto* queue = update_queue_globals();
  auto* fb = framebuffer_globals();

  // Caller (e.g. rm2fb's server, coordinating with xochitl's own separate,
  // unmodified swtcon instance via SIGSTOP + idle notices instead of this
  // lock - see tools/xochitl-mock-server) takes responsibility for
  // ensuring only one swtcon instance actually drives hardware at a time
  // when skipPidLock is set.
  if (!params.skipPidLock && create_pid_file() != 0)
    return nullptr;

  if (init_statebuffer(params.dataBuffer, params.backBuffer) != 0)
    return nullptr;

  // Phase 4 cleanup (Step 4): listProcessedUpdates/listIncomingUpdates/
  // accumList are real std::list<T> now (see update.h's
  // UpdateQueueGlobals) - their own default constructor already produces
  // a valid empty list, so the explicit self-referencing-sentinel setup
  // this used to need (back when they were a hand-rolled ListHead that
  // had to be manually made self-referencing before any empty-list check
  // or list walk would work) is no longer necessary at all.

  std::string waveform_path;
  if (params.waveformPathOverride) {
    waveform_path = params.waveformPathOverride;
  } else if (!find_waveform_path(&waveform_path)) {
    std::cerr << "swtcon_init: unable to find any waveform files!" << std::endl;
    return nullptr;
  }

  std::cout << "Calling load_waveform with path=" << waveform_path << std::endl;
  if (!load_waveform(&queue->waveform, waveform_path.c_str())) {
    std::cerr << "swtcon_init: failed to load waveform natively" << std::endl;
    return nullptr;
  }

  std::cout << "Initializing framebuffer..." << std::endl;
  FbInitParams fb_info = {};
  fb_info.xres = 0x104;
  fb_info.yres = 0x580;
  fb_info.bitsPerPixel = 0x20;
  fb_info.pixclock = 0x7080;
  fb_info.frameCount =
    kFrameSlotRingCount; // + 1 extra slot, see kInitFrameSlotIndex
  fb_info.leftMargin = 1;
  fb_info.rightMargin = 1;
  fb_info.upperMargin = 1;
  fb_info.lowerMargin = 0x8f;
  fb_info.hsyncLen = 1;
  fb_info.vsyncLen = 1;

  if (params.framebufferFd >= 0) {
    init_framebuffer_with_fd(fb_info, params.framebufferFd);
  } else if (init_framebuffer(fb_info) != 0) {
    std::cerr << "swtcon_init: failed to init framebuffer" << std::endl;
    return nullptr;
  }

  // init_framebuffer/init_framebuffer_with_fd write fb->fbFix/fbVar/nFbFd/
  // pFbMmap/nFbSizeX/nFbSizeY directly now - pan_to_frame only rewrites
  // yoffset in place, so the whole fb_var_screeninfo must be present there
  // or FBIOPAN_DISPLAY fails ("Pan failed").

  std::cout << "Calling init_LUT..." << std::endl;
  init_lut();

  std::cout << "Uploading LUT to all frame slots..." << std::endl;
  for (int i = 0; i < fb->nFbSizeY; i++) {
    upload_lut_to_frame_slot(frame_buffer_addr(i));
  }

  // Phase 7: temperature_mutex() is natively-owned zero-initialized storage
  // now (previously the library's own .bss, already valid as an
  // all-zero glibc fast mutex without an explicit init) - init it
  // explicitly like every other mutex here, before the temperature-polling
  // thread or get_current_temperature can touch it.
  pthread_mutex_init(temperature_mutex(), nullptr);
  init_temperature_sensor();

  // Prime the display exactly as qsgepaper_init does. init_framebuffer leaves
  // g_nIsFbBlanked = 1 (blanked); record the current wall-clock time in
  // timeVar; init the display-timing mutex used by the worker and display
  // threads to timestamp frame flushes; then run the priming sequence
  // prime_display (pan to frame 16, unblank, reblank). Without this the
  // frame counters never get seeded and the worker streams frame 0 forever.
  fb->nIsFbBlanked = 1;

  struct timeval tv;
  gettimeofday(&tv, nullptr);
  queue->timeVar = (int)tv.tv_sec;

  pthread_mutex_init(&queue->displayTimingMutex, nullptr);

  prime_display();

  // Needed by swtcon_lock/update/unlock_post regardless of startThreads.
  pthread_mutex_init(&queue->updateQueueMutex, nullptr);
  sem_init(&queue->displayThreadSem, 0, 0);
  pthread_mutex_init(&queue->workerCondMutex, nullptr);
  pthread_cond_init(&queue->workerCond, nullptr);

  if (params.startThreads) {
    std::cout << "Starting threads natively..." << std::endl;

    pthread_create(&queue->workerThread, nullptr, worker_thread_func, nullptr);
    pthread_create(
      &queue->displayThread, nullptr, display_thread_func, nullptr);
    queue->threadsStarted = true;

    sched_param param;
    param.__sched_priority = 99;
    pthread_setschedparam(queue->workerThread, SCHED_FIFO, &param);
    param.__sched_priority = 98;
    pthread_setschedparam(queue->displayThread, SCHED_FIFO, &param);

    // EPFramebufferSwtcon::initialize (0x38e30) calls qsgepaper_init and
    // then FUN_0003b4b4 - a startup flash of the panel, blocking until it
    // completes - before doing anything else. swtcon_init now replaces
    // that whole call site, so it does the same here.
    std::cout << "Requesting startup flash..." << std::endl;
    request_flash_and_wait();
  }

  std::cout << "swtcon_init: native initialization complete!" << std::endl;
  return (uint16_t*)queue->dataBuffer;
}

// Walk the waveform vector and print each LUT's metadata plus a checksum of
// its packed data. The ModeEntry/LUTEntry layout matches the library
// (ABI=1), so this works whether the struct was populated by
// load_waveform or the library's load_waveform, letting us A/B the
// two byte-for-byte.
void
swtcon_dump_waveform() {
  auto* vec = &update_queue_globals()->waveform;
  std::cout << "=== waveform dump: " << vec->size()
            << " modes ===" << std::endl;
  for (size_t mi = 0; mi < vec->size(); mi++) {
    ModeEntry* m = (*vec)[mi];
    std::cout << "mode[" << mi << "] name='" << m->name
              << "' luts=" << m->luts.size() << std::endl;
    for (size_t li = 0; li < m->luts.size(); li++) {
      LUTEntry* lut = m->luts[li].get();
      int uVar3 = (7 + lut->size_kb) / 8;
      int words = lut->mode_width * lut->mode_width * uVar3 + uVar3;
      size_t len = (size_t)words * 2;
      uint32_t sum = 2166136261u;
      if (lut->data) {
        uint8_t* p = (uint8_t*)lut->data;
        for (size_t b = 0; b < len; b++) {
          sum = (sum ^ p[b]) * 16777619u;
        }
      }
      std::cout << "  lut[" << li << "] size_kb=" << lut->size_kb
                << " mode_width=" << lut->mode_width
                << " temp=" << lut->temperature
                << " bit_depth=" << lut->bit_depth << " len=" << len
                << " fnv=0x" << std::hex << sum << std::dec << std::endl;
    }
  }
  std::cout << "=== end waveform dump ===" << std::endl;
}

// Checksum the fixed tables that init produces and the render kernels read, so
// they can be A/B'd between native and library init the same way as the
// waveform.
void
swtcon_dump_buffers() {
  auto* fb = framebuffer_globals();
  auto* sb = statebuffer_globals();
  struct {
    const char* name;
    void* ptr;
    size_t len;
  } bufs[] = {
    { "LUT", fb->pLUT, kLutBlobSize },
    { "gamma", sb->pGammaTable, kGammaTableSize },
    { "statebuffer", sb->pStatebuffer, kStatebufferSize },
  };
  for (auto& b : bufs) {
    uint32_t sum = 2166136261u;
    if (b.ptr) {
      uint8_t* d = (uint8_t*)b.ptr;
      for (size_t i = 0; i < b.len; i++)
        sum = (sum ^ d[i]) * 16777619u;
    }
    std::cout << "buf " << b.name << " ptr=" << b.ptr << " len=" << b.len
              << " fnv=0x" << std::hex << sum << std::dec << std::endl;
    // Also write the raw bytes to /tmp for byte-level A/B (cmp) between native
    // and library init.
    if (b.ptr) {
      std::string path = std::string("/tmp/dump_") + b.name + ".bin";
      FILE* f = fopen(path.c_str(), "wb");
      if (f) {
        fwrite(b.ptr, 1, b.len, f);
        fclose(f);
      }
    }
  }
}

// --- Re-implemented shutdown ---

void
swtcon_shutdown(uintptr_t state_ptr_or_zero) {
#if SWTCON_32BIT_ABI_CHECK
  if (swtcon_lib_impl_enabled()) {
    // Library mode: nothing native was ever allocated (swtcon_init never
    // touched update_queue_globals()), so there's no native teardown to run
    // - just call the library's own actualShutdown (0x3b6b4) by address,
    // the same way every other public entry point does in this mode.
    // qsgepaper_shutdown's own signature stays `int` (see its declaration
    // above) to match the real by-address library's own ABI exactly -
    // identical width to uintptr_t here since this path only ever compiles
    // against the real 32-bit ARM target (SWTCON_32BIT_ABI_CHECK).
    qsgepaper_shutdown((int)state_ptr_or_zero);
    std::cout << "swtcon_shutdown: complete (library mode)." << std::endl;
    return;
  }
#endif // SWTCON_32BIT_ABI_CHECK

  auto* queue = update_queue_globals();

  // Skip straight to teardown if InitParams::startThreads was false -
  // workerThread/displayThread were never pthread_create'd.
  if (queue->threadsStarted) {
    std::cout << "swtcon_shutdown: waiting for updates to complete..."
              << std::endl;

    // wait for queues to empty
    while (queue->shutdownRequested == 0 &&
           (!queue->listIncomingUpdates.empty() ||
            !queue->listProcessedUpdates.empty())) {
      usleep(100);
    }

    queue->shutdownRequested = 1;
    sem_post(&queue->displayThreadSem);
    pthread_join(queue->displayThread, nullptr);

    std::cout << "swtcon_shutdown: waiting for display to finish..."
              << std::endl;
    queue->workerThreadShutdown = 1;
    pthread_mutex_lock(&queue->workerCondMutex);
    pthread_cond_broadcast(&queue->workerCond);
    pthread_mutex_unlock(&queue->workerCondMutex);
    // Whether the worker thread is parked in swtcon_suspend()'s gate (step
    // 1) or the frame-target wait (step 5), both block on this same
    // workerCond/workerCondMutex now, so the one broadcast above reaches
    // whichever it's actually in. No need for swtcon_resume()'s
    // nIsFbBlanked=1 force here either way - both waits re-check
    // workerThreadShutdown (already set above) and exit immediately.
    pthread_join(queue->workerThread, nullptr);
  }

  if (state_ptr_or_zero != 0) {
    std::cout << "swtcon_shutdown: saving statebuffer..." << std::endl;
    save_statebuffer(state_ptr_or_zero);
    std::cout << "swtcon_shutdown: statebuffer saved" << std::endl;
  }

  std::cout << "swtcon_shutdown: shutting down..." << std::endl;

  if (is_fb_blanked() == 0) {
    blank_fb();
  }

  free_statebuffer();

  // Only free what we actually allocated - a caller-supplied dataBuffer/
  // backBuffer (see swtcon_init) is owned by the caller, e.g. rm2fb's
  // server passing in its own mmap'd shared framebuffer.
  if (queue->dataBufferOwned)
    free(queue->dataBuffer);
  if (queue->backBufferOwned)
    free(queue->backBuffer);

  close_fb();
  // dummy uninit
  free_LUT();
  unlock_pid_file();

  // Phase 7: the library's own exit-time destructor for the waveform vector
  // (FUN_000451b0 via _INIT_3) used to free every ModeEntry and the backing
  // array for us; nothing runs that once the library is never dlopen'd for
  // the production path. ModeEntry/LUTEntry already have correct native
  // destructors (see init.h), so freeing it here is just walking the
  // vector - not new reversing.
  for (auto* mode : queue->waveform)
    delete mode;
  queue->waveform.clear();

  std::cout << "swtcon_shutdown: complete." << std::endl;
}
