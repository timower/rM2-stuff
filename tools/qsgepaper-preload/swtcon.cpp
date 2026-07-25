#include "swtcon.h"
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// Symbol addresses for version 3.23.0.54 / 3.23.0.64 (assumes 0x10000 image
// base)
#define INSTANCE_ADDR 0x35de0
#define INIT_ADDR 0x3b814
#define UPDATE_ADDR 0x3ccac
#define LOCK_ADDR 0x3b690
#define UNLOCK_POST_ADDR 0x3dd90
#define WAIT_ADDR 0x3b644

typedef int (*init_func_t)(void* state_out, int zero);
typedef int (*update_func_t)(update_data* data);
typedef void (*lock_func_t)();
typedef void (*unlock_post_func_t)();
typedef void (*wait_func_t)();

static void* libqsgepaper_handle = nullptr;
static uintptr_t runtime_offset = 0;

static init_func_t qsgepaper_init = nullptr;
static update_func_t qsgepaper_update = nullptr;
static lock_func_t qsgepaper_lock = nullptr;
static unlock_post_func_t qsgepaper_unlock_post = nullptr;
static wait_func_t qsgepaper_wait = nullptr;

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
  qsgepaper_update = (update_func_t)(runtime_offset + UPDATE_ADDR);
  qsgepaper_lock = (lock_func_t)(runtime_offset + LOCK_ADDR);
  qsgepaper_unlock_post =
    (unlock_post_func_t)(runtime_offset + UNLOCK_POST_ADDR);
  qsgepaper_wait = (wait_func_t)(runtime_offset + WAIT_ADDR);
}

// Helper to resolve pointers directly
template<typename T>
T
resolve_ptr(uintptr_t addr) {
  return (T)(runtime_offset + addr);
}

#include "native_init.h"
extern void* g_pDataBufferNative;
extern void* g_pBackBufferNative;
extern int g_nFbFdNative;
extern int g_nFbSizeXNative;
extern int g_nFbSizeYNative;
extern void* g_pFbAddrNative;
extern void* g_pLUTAddrNative;

uint16_t*
swtcon_init() {
  load_lib();
  std::cout << "swtcon_init: initialization sequence starting..." << std::endl;

#if 0
    // --- NATIVE INIT IMPLEMENTATION ---
    auto dummy_func = resolve_ptr<void(*)(int)>(0x53fd0);
    auto dummy_func2 = resolve_ptr<void(*)()>(0x53bc8);
    auto dummy_func3 = resolve_ptr<void(*)()>(0x476dc);

    if (native_create_pid_file() != 0) return nullptr;

    if (native_init_statebuffer() != 0) return nullptr;

    *resolve_ptr<void**>(0x670bc) = g_pDataBufferNative;
    *resolve_ptr<void**>(0x670c0) = g_pBackBufferNative;
    *resolve_ptr<void**>(0x6d1d0) = g_pDataBufferNative;
    *resolve_ptr<int*>(0x6d1d8) = 0x503580;

    void* g_waveform_struct = resolve_ptr<void*>(0x67080);

    char* path1 = (char*)"/usr/share/remarkable/320_R467_AF4731_ED103TC2C6_VB3300-KCD_TC.wbf";

    static std::vector<ModeEntry*> native_waveform_struct;
    std::cout << "Calling native_load_waveform with path=" << path1 << std::endl;
    if (!native_load_waveform(&native_waveform_struct, path1)) {
        std::cerr << "swtcon_init: failed to load waveform natively" << std::endl;
        return nullptr;
    }

    // Sync vector structure to library memory layout
    memcpy(g_waveform_struct, &native_waveform_struct, sizeof(std::vector<ModeEntry*>));

    std::cout << "Initializing framebuffer..." << std::endl;
    int fb_info[14] = {0};
    fb_info[1] = 0x104; // xres
    fb_info[2] = 0x580; // yres
    fb_info[3] = 0x20;  // bits_per_pixel
    fb_info[4] = 0x7080; // pixclock
    fb_info[11] = 0x10; // yres_virtual / yres ?
    fb_info[5] = 1; // left_margin
    fb_info[6] = 1; // right_margin
    fb_info[7] = 1; // upper_margin
    fb_info[8] = 0x8f; // lower_margin
    fb_info[9] = 1; // hsync_len
    fb_info[10] = 1; // vsync_len

    if (native_init_framebuffer(fb_info) != 0) {
        std::cerr << "swtcon_init: failed to init framebuffer" << std::endl;
        return nullptr;
    }

    *resolve_ptr<void**>(0x6d44c) = g_pFbAddrNative;
    *resolve_ptr<int*>(0x6d440) = g_nFbSizeXNative;
    *resolve_ptr<int*>(0x6d444) = g_nFbSizeYNative;
    *resolve_ptr<int*>(0x6d358) = g_nFbFdNative;

    std::cout << "Calling init_LUT..." << std::endl;
    native_init_lut();
    *resolve_ptr<void**>(0x6d350) = g_pLUTAddrNative;

    std::cout << "Calling dummy functions..." << std::endl;
    for (int i = 0; i < 17; i++) {
        dummy_func(i);
        dummy_func2();
    }
    dummy_func3();
    std::cout << "Starting threads natively..." << std::endl;
    int* g_time_var = resolve_ptr<int*>(0x67078);
    *g_time_var = 0;

    pthread_mutex_t* g_update_queue_mutex = resolve_ptr<pthread_mutex_t*>(0x6709c);
    sem_t* g_display_thread_sem = resolve_ptr<sem_t*>(0x67068);
    pthread_mutex_t* g_worker_cond_mutex = resolve_ptr<pthread_mutex_t*>(0x66fec);
    pthread_cond_t* g_worker_cond = resolve_ptr<pthread_cond_t*>(0x67008);

    pthread_mutex_init(g_update_queue_mutex, nullptr);
    sem_init(g_display_thread_sem, 0, 0);
    pthread_mutex_init(g_worker_cond_mutex, nullptr);
    pthread_cond_init(g_worker_cond, nullptr);

    pthread_t* g_worker_thread = resolve_ptr<pthread_t*>(0x670b8);
    pthread_t* g_display_thread = resolve_ptr<pthread_t*>(0x670b4);

    auto worker_thread_func = resolve_ptr<void*(*)(void*)>(0x3ae38);
    auto display_thread_func = resolve_ptr<void*(*)(void*)>(0x3d2ac);

    pthread_create(g_worker_thread, nullptr, worker_thread_func, nullptr);
    pthread_create(g_display_thread, nullptr, display_thread_func, nullptr);

    sched_param param;
    param.__sched_priority = 99;
    pthread_setschedparam(*g_worker_thread, SCHED_FIFO, &param);
    param.__sched_priority = 98;
    pthread_setschedparam(*g_display_thread, SCHED_FIFO, &param);

    std::cout << "swtcon_init: native initialization complete!" << std::endl;
    return (uint16_t*)g_pDataBufferNative;

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

void
swtcon_lock() {
  qsgepaper_lock();
}
void
swtcon_update(update_data* data) {
  qsgepaper_update(data);
}
void
swtcon_unlock_post() {
  qsgepaper_unlock_post();
}
void
swtcon_wait() {
  qsgepaper_wait();
}

// --- Re-implemented subroutines ---

void
native_save_statebuffer(int state_ptr_or_zero) {
  void** g_pStatebufferPtr = resolve_ptr<void**>(0x6d1d0);
  int* g_nStatebufferSize = resolve_ptr<int*>(0x6d1d8);
  char* filename = (char*)state_ptr_or_zero;
  FILE* f = fopen64(filename, "w");
  if (f) {
    fwrite(*g_pStatebufferPtr, *g_nStatebufferSize, 1, f);
    fclose(f);
  }
}

int
native_is_fb_blanked() {
  return *resolve_ptr<int*>(0x6d448);
}

void
native_blank_fb() {
  int* g_nIsFbBlanked = resolve_ptr<int*>(0x6d448);
  int* g_nFbFd = resolve_ptr<int*>(0x6d358);
  if (*g_nIsFbBlanked == 0) {
    *g_nIsFbBlanked = 1;
    // FBIOBLANK = 0x4611, FB_BLANK_POWERDOWN = 4
    if (ioctl(*g_nFbFd, 0x4611, 4) == -1) {
      std::cerr << "FBIOBLANK failed (blank)" << std::endl;
    }
  }
}

void
native_free_statebuffer() {
  void** g_pStatebufferPtr = resolve_ptr<void**>(0x6d1d0);
  void** g_pStatebufferObj = resolve_ptr<void**>(0x6d1d4);
  free(*g_pStatebufferPtr);
  if (*g_pStatebufferObj != nullptr) {
    ::operator delete(*g_pStatebufferObj);
  }
}

void
native_close_fb() {
  void** g_pFbMmapPtr = resolve_ptr<void**>(0x6d44c);
  int* g_nFbSizeX = resolve_ptr<int*>(0x6d440);
  int* g_nFbSizeY = resolve_ptr<int*>(0x6d444);
  int* g_nFbFd = resolve_ptr<int*>(0x6d358);

  munmap(*g_pFbMmapPtr, (*g_nFbSizeX) * (*g_nFbSizeY));
  *g_pFbMmapPtr = nullptr;
  close(*g_nFbFd);
  *g_nFbFd = -1;
}

void
native_free_LUT() {
  void** g_pLutPtr = resolve_ptr<void**>(0x6d350);
  if (*g_pLutPtr != nullptr) {
    ::operator delete(*g_pLutPtr);
  }
}

void
native_unlock_pid_file() {
  int* g_nPidFd = resolve_ptr<int*>(0x66dec);
  if (*g_nPidFd > -1) {
    // LOCK_UN = 8
    if (flock(*g_nPidFd, 8) == -1) {
      std::cerr << "unable to unlock exclusive lock" << std::endl;
    }
    close(*g_nPidFd);
  }
}

// --- Re-implemented shutdown ---

void
swtcon_shutdown(int state_ptr_or_zero) {
  std::cout << "swtcon_shutdown: waiting for updates to complete..."
            << std::endl;

  // Resolve globals needed by shutdown (using original offsets based on 0x10000
  // image base)
  int* g_nShutdownRequested = resolve_ptr<int*>(0x6708c);
  void** g_list_incoming_updates = resolve_ptr<void**>(0x67090);
  void** g_list_processed_updates = resolve_ptr<void**>(0x66fd8);
  sem_t* g_display_thread_sem = resolve_ptr<sem_t*>(0x67068);
  pthread_t* g_display_thread = resolve_ptr<pthread_t*>(0x670b4);

  int* g_nWorkerThreadShutdown = resolve_ptr<int*>(0x6707c);
  pthread_mutex_t* g_worker_cond_mutex = resolve_ptr<pthread_mutex_t*>(0x66fec);
  pthread_cond_t* g_worker_cond = resolve_ptr<pthread_cond_t*>(0x67008);
  pthread_t* g_worker_thread = resolve_ptr<pthread_t*>(0x670b8);

  void** g_data_buffer = resolve_ptr<void**>(0x670bc); // data buffer
  void** g_back_buffer = resolve_ptr<void**>(0x670c0); // back buffer

  // wait for queues to empty
  while (*g_nShutdownRequested == 0 &&
         (*g_list_incoming_updates != (void*)g_list_incoming_updates ||
          *g_list_processed_updates != (void*)g_list_processed_updates)) {
    usleep(100);
  }

  *g_nShutdownRequested = 1;
  sem_post(g_display_thread_sem);
  pthread_join(*g_display_thread, nullptr);

  std::cout << "swtcon_shutdown: waiting for display to finish..." << std::endl;
  *g_nWorkerThreadShutdown = 1;
  pthread_mutex_lock(g_worker_cond_mutex);
  pthread_cond_broadcast(g_worker_cond);
  pthread_mutex_unlock(g_worker_cond_mutex);
  pthread_join(*g_worker_thread, nullptr);

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

  free(*g_data_buffer);
  free(*g_back_buffer);

  native_close_fb();
  // dummy uninit
  native_free_LUT();
  native_unlock_pid_file();

  std::cout << "swtcon_shutdown: complete." << std::endl;
}
