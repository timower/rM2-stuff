#include "swtcon.h"
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <sys/file.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "native_display.h"
#include "native_init.h"
#include "qsgepaper_globals.h"

// Symbol addresses for version 3.23.0.54 / 3.23.0.64 (assumes 0x10000 image
// base)
#define INSTANCE_ADDR 0x35de0
#define INIT_ADDR 0x3b814
#define UPDATE_ADDR 0x3ccac
#define LOCK_ADDR 0x3b690
#define UNLOCK_POST_ADDR 0x3dd90
#define WAIT_ADDR 0x3b644

typedef int (*init_func_t)(void* state_out, int zero);

static void* libqsgepaper_handle = nullptr;
static uintptr_t runtime_offset = 0;

// Owns the waveform vector's backing allocation so swtcon_shutdown can free
// it (Phase 7) - see the comment at its allocation site in swtcon_init.
static std::vector<ModeEntry*>* g_nativeWaveformStruct = nullptr;

static init_func_t qsgepaper_init = nullptr;

// Library exports for A/B comparison against the native update
// reimplementation - see native_update.cpp's NATIVE_UPDATE switch. Non-static
// (extern) so native_update.cpp can reach them for its `#else` fallback.
void (*qsgepaper_lock)() = nullptr;
void (*qsgepaper_update)(update_data*) = nullptr;
void (*qsgepaper_unlock_post)() = nullptr;
void (*qsgepaper_wait)() = nullptr;

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
}

uintptr_t
swtcon_runtime_offset() {
  return runtime_offset;
}

extern void* g_pImageBufferNative;
extern void* g_pScreenBufferNative;
extern void* g_pStateBufferNative;
extern void* g_pGammaTableNative;
extern int g_nFbFdNative;
extern int g_nFbSizeXNative;
extern int g_nFbSizeYNative;
extern void* g_pFbAddrNative;
extern void* g_pLUTAddrNative;
extern struct fb_var_screeninfo g_fbVarScreeninfoNative;
extern struct fb_fix_screeninfo g_fbFixScreeninfoNative;

uint16_t*
swtcon_init() {
  load_lib();
  std::cout << "swtcon_init: initialization sequence starting..." << std::endl;

#if 1
    // --- NATIVE INIT IMPLEMENTATION ---
    if (native_create_pid_file() != 0) return nullptr;

    if (native_init_statebuffer() != 0) return nullptr;

    auto* queue = update_queue_globals();
    auto* sb = statebuffer_globals();
    auto* fb = framebuffer_globals();

    // Phase 7: these three intrusive std::list sentinels used to come from
    // the library's own .bss, already self-referencing by the time our code
    // ran because the library's C++ static constructors initialized them at
    // dlopen() time. Now that UpdateQueueGlobals is natively-owned storage
    // (zero-initialized), nothing constructs them - so every empty-list
    // check ("next == &head") and every list walk needs this explicit
    // self-reference set up before anything else touches the queue.
    queue->listProcessedUpdates = { &queue->listProcessedUpdates, &queue->listProcessedUpdates };
    queue->listIncomingUpdates = { &queue->listIncomingUpdates, &queue->listIncomingUpdates };
    queue->accumList = { &queue->accumList, &queue->accumList };

    queue->dataBuffer = g_pImageBufferNative;
    queue->backBuffer = g_pScreenBufferNative;
    sb->pStatebuffer = g_pStateBufferNative;
    sb->pGammaTable = g_pGammaTableNative;
    sb->nSize = 0x503580;

    char* path1 = (char*)"/usr/share/remarkable/320_R467_AF4731_ED103TC2C6_VB3300-KCD_TC.wbf";

    // Heap-allocated; torn down by swtcon_shutdown (Phase 7 - see
    // g_nativeWaveformStruct's declaration). queue->waveformStructRaw below
    // gets a byte-copy of this vector's {begin,end,cap} pointers so it aliases
    // the exact same backing array - only the 12-byte vector object itself is
    // a separate allocation, kept alive here so shutdown has something to
    // free it through.
    g_nativeWaveformStruct = new std::vector<ModeEntry*>();
    std::cout << "Calling native_load_waveform with path=" << path1 << std::endl;
    if (!native_load_waveform(g_nativeWaveformStruct, path1)) {
        std::cerr << "swtcon_init: failed to load waveform natively" << std::endl;
        return nullptr;
    }

    // Sync vector structure to library memory layout
    static_assert(sizeof(queue->waveformStructRaw) == sizeof(std::vector<ModeEntry*>), "");
    memcpy(queue->waveformStructRaw, g_nativeWaveformStruct, sizeof(std::vector<ModeEntry*>));

    std::cout << "Initializing framebuffer..." << std::endl;
    FbInitParams fb_info = {};
    fb_info.xres = 0x104;
    fb_info.yres = 0x580;
    fb_info.bitsPerPixel = 0x20;
    fb_info.pixclock = 0x7080;
    fb_info.frameCount = 0x10;
    fb_info.leftMargin = 1;
    fb_info.rightMargin = 1;
    fb_info.upperMargin = 1;
    fb_info.lowerMargin = 0x8f;
    fb_info.hsyncLen = 1;
    fb_info.vsyncLen = 1;

    if (native_init_framebuffer(fb_info) != 0) {
        std::cerr << "swtcon_init: failed to init framebuffer" << std::endl;
        return nullptr;
    }

    fb->pFbMmap = g_pFbAddrNative;
    fb->nFbSizeX = g_nFbSizeXNative;
    fb->nFbSizeY = g_nFbSizeYNative;
    fb->nFbFd = g_nFbFdNative;

    // The library's pan/display code reads the full fb_var_screeninfo /
    // fb_fix_screeninfo from these globals. pan_to_frame only rewrites yoffset
    // in place, so the whole struct must be present or FBIOPAN_DISPLAY fails
    // ("Pan failed").
    fb->fbFix = g_fbFixScreeninfoNative;
    fb->fbVar = g_fbVarScreeninfoNative;

    std::cout << "Calling init_LUT..." << std::endl;
    native_init_lut();
    fb->pLUT = g_pLUTAddrNative;

    std::cout << "Uploading LUT to all frame slots..." << std::endl;
    for (int i = 0; i < g_nFbSizeYNative; i++) {
        native_upload_lut_to_frame_slot(native_frame_buffer_addr(i));
    }

    // Phase 7: temperature_mutex() is natively-owned zero-initialized storage
    // now (previously the library's own .bss, already valid as an
    // all-zero glibc fast mutex without an explicit init) - init it
    // explicitly like every other mutex here, before the temperature-polling
    // thread or native_get_current_temperature can touch it.
    pthread_mutex_init(temperature_mutex(), nullptr);
    native_init_temperature_sensor();

    // Prime the display exactly as qsgepaper_init does. init_framebuffer leaves
    // g_nIsFbBlanked = 1 (blanked); record the current wall-clock time in
    // timeVar; init the display-timing mutex used by the worker and display
    // threads to timestamp frame flushes; then run the priming sequence
    // native_prime_display (pan to frame 16, unblank, reblank). Without this the
    // frame counters never get seeded and the worker streams frame 0 forever.
    fb->nIsFbBlanked = 1;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    queue->timeVar = (int)tv.tv_sec;

    pthread_mutex_init(&queue->displayTimingMutex, nullptr);

    native_prime_display();

    std::cout << "Starting threads natively..." << std::endl;

    pthread_mutex_init(&queue->updateQueueMutex, nullptr);
    sem_init(&queue->displayThreadSem, 0, 0);
    pthread_mutex_init(&queue->workerCondMutex, nullptr);
    pthread_cond_init(&queue->workerCond, nullptr);

    pthread_create(&queue->workerThread, nullptr, native_worker_thread_func, nullptr);
    pthread_create(&queue->displayThread, nullptr, native_display_thread_func, nullptr);

    // auto display_thread_func = resolve_ptr<void*(*)(void*)>(0x3d2ac);
    // pthread_create(&queue->displayThread, nullptr, display_thread_func, nullptr);

    sched_param param;
    param.__sched_priority = 99;
    pthread_setschedparam(queue->workerThread, SCHED_FIFO, &param);
    param.__sched_priority = 98;
    pthread_setschedparam(queue->displayThread, SCHED_FIFO, &param);

    // EPFramebufferSwtcon::initialize (0x38e30) calls qsgepaper_init and then
    // FUN_0003b4b4 - a startup flash of the panel, blocking until it
    // completes - before doing anything else. swtcon_init now replaces that
    // whole call site, so it does the same here.
    std::cout << "Requesting startup flash..." << std::endl;
    native_request_flash_and_wait();

    std::cout << "swtcon_init: native initialization complete!" << std::endl;
    return (uint16_t*)g_pImageBufferNative;

#else
  char state[256];
  memset(state, 0, sizeof(state));

  std::cout << "Initializing swtcon..." << std::endl;
  int init_res = qsgepaper_init(state, 0);
  if (init_res != 0) {
    std::cerr << "Error initializing swtcon, res: " << init_res << std::endl;
    return nullptr;
  }
  uint16_t* image = *(uint16_t**)(state + 0x14);
  return image;

#endif
}

// Walk the waveform vector and print each LUT's metadata plus a checksum of
// its packed data. The ModeEntry/LUTEntry layout matches the library
// (ABI=1), so this works whether the struct was populated by
// native_load_waveform or the library's load_waveform, letting us A/B the
// two byte-for-byte.
void
swtcon_dump_waveform() {
  auto* vec = (std::vector<ModeEntry*>*)(void*)update_queue_globals()->waveformStructRaw;
  std::cout << "=== waveform dump: " << vec->size() << " modes ===" << std::endl;
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
// they can be A/B'd between native and library init the same way as the waveform.
void
swtcon_dump_buffers() {
  auto* fb = framebuffer_globals();
  auto* sb = statebuffer_globals();
  struct {
    const char* name;
    void* ptr;
    size_t len;
  } bufs[] = {
    { "LUT", fb->pLUT, 0x165800 },
    { "gamma", sb->pGammaTable, 0x4400 },
    { "statebuffer", sb->pStatebuffer, 0x503580 },
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
swtcon_shutdown(int state_ptr_or_zero) {
  std::cout << "swtcon_shutdown: waiting for updates to complete..."
            << std::endl;

  auto* queue = update_queue_globals();

  // wait for queues to empty
  while (queue->shutdownRequested == 0 &&
         (queue->listIncomingUpdates.next != &queue->listIncomingUpdates ||
          queue->listProcessedUpdates.next != &queue->listProcessedUpdates)) {
    usleep(100);
  }

  queue->shutdownRequested = 1;
  sem_post(&queue->displayThreadSem);
  pthread_join(queue->displayThread, nullptr);

  std::cout << "swtcon_shutdown: waiting for display to finish..." << std::endl;
  queue->workerThreadShutdown = 1;
  pthread_mutex_lock(&queue->workerCondMutex);
  pthread_cond_broadcast(&queue->workerCond);
  pthread_mutex_unlock(&queue->workerCondMutex);
  pthread_join(queue->workerThread, nullptr);

  if (state_ptr_or_zero != 0) {
    std::cout << "swtcon_shutdown: saving statebuffer..." << std::endl;
    native_save_statebuffer(state_ptr_or_zero);
    std::cout << "swtcon_shutdown: statebuffer saved" << std::endl;
  }

  std::cout << "swtcon_shutdown: shutting down..." << std::endl;

  if (native_is_fb_blanked() == 0) {
    native_blank_fb();
  }

  native_free_statebuffer();

  free(queue->dataBuffer);
  free(queue->backBuffer);

  native_close_fb();
  // dummy uninit
  native_free_LUT();
  native_unlock_pid_file();

  // Phase 7: the library's own exit-time destructor for the waveform vector
  // (FUN_000451b0 via _INIT_3) used to free every ModeEntry and the backing
  // array for us; nothing runs that once the library is never dlopen'd for
  // the production path. ModeEntry/LUTEntry already have correct native
  // destructors (see native_init.h), so freeing it here is just walking the
  // vector - not new reversing.
  if (g_nativeWaveformStruct) {
    for (auto* mode : *g_nativeWaveformStruct)
      delete mode;
    delete g_nativeWaveformStruct;
    g_nativeWaveformStruct = nullptr;
  }

  std::cout << "swtcon_shutdown: complete." << std::endl;
}
