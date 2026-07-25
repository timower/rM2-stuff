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

uintptr_t
swtcon_runtime_offset() {
  return runtime_offset;
}

// Helper to resolve pointers directly
template<typename T>
T
resolve_ptr(uintptr_t addr) {
  return (T)(runtime_offset + addr);
}

#include "native_init.h"
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

// Forward declarations: implementations live later in this file, in the
// "Re-implemented subroutines" section, but are needed by native_prime_display
// below.
int native_is_fb_blanked();
void native_blank_fb();

// Mirrors frame_buffer_addr (0x53fd0): address of frame slot `frame_idx`
// within the mmap'd framebuffer (each slot is g_nFbSizeXNative bytes).
static void*
native_frame_buffer_addr(int frame_idx) {
  return (uint8_t*)g_pFbAddrNative + (size_t)g_nFbSizeXNative * frame_idx;
}

// Mirrors upload_lut_to_frame_slot (0x53bc8): copies the full waveform LUT
// table into one hardware-visible frame slot. The panel controller DMA-reads
// the LUT straight out of the mapped framebuffer memory, so every frame slot
// needs its own copy.
static void
native_upload_lut_to_frame_slot(void* dest) {
  memcpy(dest, g_pLUTAddrNative, 0x165800);
}

// Path discovered once by native_init_temperature_sensor and reused by every
// subsequent native_refresh_temperature_cache call.
static std::string g_hwmonTempPathNative;

// Mirrors refresh_temperature_cache (0x4681c): reads the hwmon sensor at the
// discovered path (unconditionally, even if empty — matching the library,
// which doesn't special-case a failed discovery here and just lets the open
// fail with its own error message), subtracts the fixed 2.0C calibration
// offset, and stores the result into the library's cached-temperature global
// under its mutex. No-op if the read fails (leaves whatever value was cached
// before).
static void
native_refresh_temperature_cache() {
  int raw_value;
  if (!native_read_temperature_raw(g_hwmonTempPathNative.c_str(), &raw_value))
    return;

  pthread_mutex_t* mutex = resolve_ptr<pthread_mutex_t*>(0x6d180); // g_dwTemperatureMutex
  float* cached_temp = resolve_ptr<float*>(0x66e20);               // g_flCachedTemperature
  pthread_mutex_lock(mutex);
  *cached_temp = (float)raw_value - 2.0f;
  pthread_mutex_unlock(mutex);
}

// Mirrors init_temperature_sensor (0x476dc): discovers the sy7636a_temperature
// hwmon sysfs path once, then performs an initial cache refresh so
// get_current_temperature has a valid reading before the display/worker
// threads start.
static void
native_init_temperature_sensor() {
  if (native_find_temperature_hwmon_path(&g_hwmonTempPathNative)) {
    std::cout << "temperature_hwmon: found temperature path: "
              << g_hwmonTempPathNative << std::endl;
  } else {
    std::cerr << "temperature_hwmon: failed to find path" << std::endl;
  }
  native_refresh_temperature_cache();
}

// Mirrors pan_and_unblank (0x53ebc): writes the given frame slot's yoffset
// into the library's fb_var_screeninfo and issues FBIOPUT_VSCREENINFO, then
// retries an FBIOBLANK unblank up to 5 times. If unblanking never succeeds,
// falls back to a lightweight FBIOPAN_DISPLAY to the last frame slot as a
// last resort (and still reports failure either way).
static int
native_pan_and_unblank(int frame_idx) {
  int* g_nIsFbBlanked = resolve_ptr<int*>(0x6d448);
  int* g_nFbFd = resolve_ptr<int*>(0x6d358);
  auto* vinfo = resolve_ptr<struct fb_var_screeninfo*>(0x6d3a0);

  if (*g_nIsFbBlanked == 0)
    return 1;

  vinfo->yoffset = frame_idx * vinfo->yres;
  if (ioctl(*g_nFbFd, FBIOPUT_VSCREENINFO, vinfo) == -1) {
    std::cerr << "Panning failed! start buffer = " << frame_idx << ", "
              << strerror(errno) << std::endl;
    return -1;
  }

  for (int retry = 5; retry > 0; retry--) {
    if (ioctl(*g_nFbFd, FBIOBLANK, FB_BLANK_UNBLANK) != -1) {
      *g_nIsFbBlanked = 0;
      return 0;
    }
    std::cout << "FBIOBLANK failed (unblank)" << std::endl;
  }

  vinfo->yoffset = g_nFbSizeYNative * vinfo->yres;
  if (ioctl(*g_nFbFd, FBIOPAN_DISPLAY, vinfo) == -1) {
    std::cout << "Pan failed" << std::endl;
  }
  return -1;
}

// Mirrors prime_display (0x468f0). Called once from swtcon_init right after
// g_nIsFbBlanked is forced to 1: briefly unblanks frame slot 16 so the panel
// controller has a primed frame, refreshes the temperature cache, then
// reblanks. Without this the frame counters are never seeded and the display
// thread never produces a first visible refresh.
static void
native_prime_display() {
  if (native_is_fb_blanked() == 0) {
    native_refresh_temperature_cache();
    return;
  }
  native_pan_and_unblank(16);
  native_refresh_temperature_cache();
  native_blank_fb();
}

uint16_t*
swtcon_init() {
  load_lib();
  std::cout << "swtcon_init: initialization sequence starting..." << std::endl;

#if 1
    // --- NATIVE INIT IMPLEMENTATION ---
    if (native_create_pid_file() != 0) return nullptr;

    if (native_init_statebuffer() != 0) return nullptr;

    *resolve_ptr<void**>(0x670bc) = g_pImageBufferNative;  // g_pDataBuffer
    *resolve_ptr<void**>(0x670c0) = g_pScreenBufferNative; // g_pBackBuffer
    *resolve_ptr<void**>(0x6d1d0) = g_pStateBufferNative;  // statebuffer
    *resolve_ptr<void**>(0x6d1d4) = g_pGammaTableNative;   // gamma/temp table
    *resolve_ptr<int*>(0x6d1d8) = 0x503580;

    void* g_waveform_struct = resolve_ptr<void*>(0x67080);

    char* path1 = (char*)"/usr/share/remarkable/320_R467_AF4731_ED103TC2C6_VB3300-KCD_TC.wbf";

    // Heap-allocated and intentionally leaked: the library registers an exit-time
    // destructor for g_waveform_struct (FUN_000451b0 via _INIT_3) that frees every
    // ModeEntry and the backing array. This handle shares that same array after
    // the memcpy below, so it must NOT also free it — hence the deliberate leak
    // (of the 12-byte vector object only; the contents are owned by
    // g_waveform_struct).
    auto* native_waveform_struct = new std::vector<ModeEntry*>();
    std::cout << "Calling native_load_waveform with path=" << path1 << std::endl;
    if (!native_load_waveform(native_waveform_struct, path1)) {
        std::cerr << "swtcon_init: failed to load waveform natively" << std::endl;
        return nullptr;
    }

    // Sync vector structure to library memory layout
    memcpy(g_waveform_struct, native_waveform_struct, sizeof(std::vector<ModeEntry*>));

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

    // The library's pan/display code reads the full fb_var_screeninfo /
    // fb_fix_screeninfo from these globals (g_fbFixScreeninfo @0x6d35c,
    // g_fbVarScreeninfo @0x6d3a0). pan_to_frame only rewrites yoffset in place,
    // so the whole struct must be present or FBIOPAN_DISPLAY fails ("Pan failed").
    memcpy(resolve_ptr<void*>(0x6d35c), &g_fbFixScreeninfoNative,
           sizeof(g_fbFixScreeninfoNative));
    memcpy(resolve_ptr<void*>(0x6d3a0), &g_fbVarScreeninfoNative,
           sizeof(g_fbVarScreeninfoNative));

    std::cout << "Calling init_LUT..." << std::endl;
    native_init_lut();
    *resolve_ptr<void**>(0x6d350) = g_pLUTAddrNative;

    std::cout << "Uploading LUT to all frame slots..." << std::endl;
    for (int i = 0; i < g_nFbSizeYNative; i++) {
        native_upload_lut_to_frame_slot(native_frame_buffer_addr(i));
    }

    native_init_temperature_sensor();

    // Prime the display exactly as qsgepaper_init does. init_framebuffer leaves
    // g_nIsFbBlanked = 1 (blanked); record the current wall-clock time in
    // g_time_var; init the display-timing mutex (0x6703c) used by the worker and
    // display threads to timestamp frame flushes; then run the priming sequence
    // native_prime_display (pan to frame 16, unblank, reblank). Without this the
    // frame counters never get seeded and the worker streams frame 0 forever.
    *resolve_ptr<int*>(0x6d448) = 1; // g_nIsFbBlanked

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int* g_time_var = resolve_ptr<int*>(0x67078);
    *g_time_var = (int)tv.tv_sec;

    pthread_mutex_t* g_display_timing_mutex =
      resolve_ptr<pthread_mutex_t*>(0x6703c);
    pthread_mutex_init(g_display_timing_mutex, nullptr);

    native_prime_display();

    std::cout << "Starting threads natively..." << std::endl;

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

// Set to 1 to use the native re-implementations of update/lock/unlock/wait;
// 0 falls back to the library exports (for A/B comparison).
#define NATIVE_UPDATE 1

// --- Native swtcon_update / lock / unlock_post / wait ---
//
// These mirror queue_update (0x3ccac), LockSwapMutex (0x3b690),
// unlock_and_post_swap / UnlockAndPostSwapMutex (0x3dd90) and WaitForUpdate
// (0x3b644). The native code owns the control flow, work-item field packing and
// the intrusive std::list / std::shared_ptr book-keeping. `update_item_ctor`,
// `clamp_update_rect`, `get_current_temperature`, `update_item_copy` and
// `free_update_region_list` are now reimplemented natively (see below); the
// remaining leaf routines are still called into libqsgepaper by address (to
// be reversed in later steps):
//   0x4fff8 dispatch_update_regions - diff image vs back buffer -> region rows
//   0x4535c select_waveform_lut     - pick the LUT shared_ptr for temp+mode
//   0x409e4 update_lut_is_valid     - sanity-check the selected LUT dims
//   0x3be10 subtract_update_region  - clip this rect out of pending regions
//   0x3ea98 build_update_batch      - clone accumulation list into a batch node
//   0x1ec58 _M_hook                 - std::__detail::_List_node_base::_M_hook
//   0x408a8 (anonymous)             - allocate an empty LUTEntry + shared_ptr
//                                     control block (used for the +0x2c
//                                     placeholder in update_item_ctor, always
//                                     immediately replaced by select_waveform_lut)
//
// Work-item layout (0x5c bytes, lives at node+8 in a 100-byte list node):
//   +0x00 shared_ptr  (region rows produced by dispatch_update_regions)
//   +0x0c int rect[4] (y0,x0,y1,x1)   +0x1c seq id
//   +0x28/+0x2a shorts (0, LUT width-1)
//   +0x2c shared_ptr  (selected waveform LUT)   +0x34 short mode
//   +0x38 float temp  +0x3c shared_ptr
//   +0x48 std::list<int> head (0xc-byte nodes)  +0x50 int count
//   +0x54 byte sync, +0x55 byte full-refresh, +0x58 int pixel_mode

// Release a libstdc++ _Sp_counted_base* exactly like the inlined shared_ptr
// destructor in queue_update: atomically drop the use-count, dispose on 0, then
// drop the weak-count and destroy on 0. vtable[2]=_M_dispose, vtable[3]=_M_destroy.
static void
release_sp(void* ctrl_) {
  if (!ctrl_)
    return;
  int* ctrl = (int*)ctrl_;
  void** vt = *(void***)ctrl;
  if (__atomic_fetch_sub(&ctrl[1], 1, __ATOMIC_ACQ_REL) == 1) {
    ((void (*)(void*))vt[2])(ctrl);
    if (__atomic_fetch_sub(&ctrl[2], 1, __ATOMIC_ACQ_REL) == 1) {
      ((void (*)(void*))vt[3])(ctrl);
    }
  }
}

// Retain a libstdc++ _Sp_counted_base* by atomically incrementing its
// use-count (ctrl+4), mirroring release_sp's decrement side. Used when a
// shared_ptr is copied (not moved) into a new work item.
static void
retain_sp(void* ctrl_) {
  if (!ctrl_)
    return;
  int* ctrl = (int*)ctrl_;
  __atomic_fetch_add(&ctrl[1], 1, __ATOMIC_ACQ_REL);
}

// Native reimplementation of free_update_region_list (0x3e540): destroys and
// frees every work-item node in a circular intrusive list (used for both the
// pending-accumulation list and a claimed batch's region list). For each
// 100-byte node (item lives at node+8): frees the embedded std::list<int> at
// item+0x48, releases the three shared_ptrs (+0x00, +0x2c, +0x3c - exactly
// the same trio release_sp already handles in swtcon_update's inline
// work-item destructor), then frees the node itself.
static void
native_free_update_region_list(void* list_head_) {
  void** head = (void**)list_head_;
  void* node = head[0];
  if (node == (void*)head)
    return;
  do {
    void* next = *(void**)node;
    char* item = (char*)node + 8;

    void** sub = (void**)(item + 0x48);
    for (void* n = sub[0]; n != (void*)sub;) {
      void* nx = *(void**)n;
      ::operator delete(n);
      n = nx;
    }
    release_sp(*(void**)(item + 0x40)); // +0x3c shared_ptr
    release_sp(*(void**)(item + 0x30)); // +0x2c shared_ptr (LUT)
    release_sp(*(void**)(item + 0x04)); // +0x00 shared_ptr

    ::operator delete(node);
    node = next;
  } while (node != (void*)head);
}

// Native reimplementation of update_item_copy (0x3e850): deep-copies a work
// item into a fresh destination, retaining (incrementing use_count on) the
// three shared_ptrs rather than moving them, deep-copying the embedded
// std::list<int> at +0x48 (new 0xc-byte nodes carrying the same int
// payloads, relinked via a plain circular-list append), and copying every
// other scalar field verbatim (rect, sequence id, LUT-width shorts, mode,
// temperature, sync/full-refresh flags, pixel_mode).
static void*
native_update_item_copy(void* dest_, const void* src_) {
  char* D = (char*)dest_;
  const char* S = (const char*)src_;

  memcpy(D, S, 0x2c); // shared_ptr#1, gap, rect, seq id, +0x24, LUT-width shorts
  retain_sp(*(void**)(D + 0x04));

  memcpy(D + 0x2c, S + 0x2c, 8); // shared_ptr#2 (selected LUT)
  retain_sp(*(void**)(D + 0x30));

  memcpy(D + 0x34, S + 0x34, 8); // mode (+0x34), temp (+0x38)

  memcpy(D + 0x3c, S + 0x3c, 8); // shared_ptr#3
  retain_sp(*(void**)(D + 0x40));

  *(int32_t*)(D + 0x44) = *(const int32_t*)(S + 0x44);

  void** dest_head = (void**)(D + 0x48);
  dest_head[0] = dest_head;
  dest_head[1] = dest_head;
  int count = 0;
  void* const* src_head = (void* const*)(S + 0x48);
  for (void* n = src_head[0]; n != (void*)src_head; n = *(void* const*)n) {
    int value = *(const int32_t*)((const char*)n + 8);
    void* node = ::operator new(0xc);
    *(int32_t*)((char*)node + 8) = value;
    void** new_node = (void**)node;
    void* tail = dest_head[1];
    new_node[0] = dest_head;
    new_node[1] = tail;
    ((void**)tail)[0] = node;
    dest_head[1] = node;
    count++;
  }
  *(int32_t*)(D + 0x50) = count;

  *(int32_t*)(D + 0x54) = *(const int32_t*)(S + 0x54);
  *(int32_t*)(D + 0x58) = *(const int32_t*)(S + 0x58);

  return dest_;
}

// Native reimplementation of update_item_ctor (0x3ffd0): zero-initialises the
// work item to a degenerate/empty rect {y0=0,x0=0,y1=-1,x1=-1}, a 25C default
// temperature, pixel_mode=5, and an empty self-referencing intrusive list
// head. Stamps the same global sequence counter the library itself uses at
// +0x1c (DAT_0006d178) so IDs stay unique. The +0x2c placeholder LUT
// shared_ptr allocation is left to the library's own tiny allocator
// (0x408a8) - it's always released and replaced by select_waveform_lut
// before anything reads it, so there's no benefit to reversing it further.
static void*
native_update_item_ctor(void* item_) {
  unsigned char* I = (unsigned char*)item_;
  memset(I, 0, 0x5c);
  *(int32_t*)(I + 0x14) = -1; // rect[2] (y1)
  *(int32_t*)(I + 0x18) = -1; // rect[3] (x1)
  *(float*)(I + 0x38) = 25.0f;
  void** list_head = (void**)(I + 0x48);
  list_head[0] = list_head;
  list_head[1] = list_head;
  *(int32_t*)(I + 0x58) = 5; // default pixel_mode

  // Real signature is (void* out_sp, int size_kb, int mode_width, int
  // bit_depth, float temperature) - `temperature` is passed in a VFP
  // register (s0) per AAPCS-VFP, not as a 5th integer arg. size_kb=0 skips
  // the data-buffer allocation, so the other fields are irrelevant here:
  // this placeholder is always released and replaced by select_waveform_lut
  // before anything reads it.
  auto fn_make_empty_lut = resolve_ptr<void* (*)(void*, int, int, int, float)>(0x408a8);
  fn_make_empty_lut(I + 0x2c, 0, 0, 0, 0.0f);

  int* seq_counter = resolve_ptr<int*>(0x6d178);
  int seq = ++(*seq_counter);
  *(int32_t*)(I + 0x1c) = seq;
  return item_;
}

// Native reimplementation of clamp_update_rect (0x4fc40). Input is
// {x0,y0,x1,y1} (queue_update passes update_data's x/y/width/height reordered
// this way - "width"/"height" are actually the opposite-corner coordinates,
// not sizes: main.cpp's full-screen requests set them to SCREEN_WIDTH/
// SCREEN_HEIGHT, matching the 1403/1871 constants below exactly). The
// transform is an independent per-axis point reflection through
// (SCREEN_HEIGHT-1, SCREEN_WIDTH-1) - i.e. it flips the rect into the
// panel's 180-rotated hardware frame - with the y-axis rounded to 8-row
// blocks (down for y0, up for y1) to match the 8-row-aligned render/dispatch
// kernels. Output order is {y0,x0,y1,x1}.
static void
native_clamp_update_rect(int* rect_out, const int* rect_in) {
  constexpr int kMaxY = 0x74f; // SCREEN_HEIGHT - 1 = 1871
  constexpr int kMaxX = 0x57b; // SCREEN_WIDTH - 1 = 1403
  int x0 = rect_in[0], y0 = rect_in[1], x1 = rect_in[2], y1 = rect_in[3];

  auto non_neg = [](int v) { return v < 0 ? 0 : v; };
  auto clamp_hi = [](int v, int hi) { return v > hi ? hi : v; };

  rect_out[0] = non_neg(kMaxY - y1) & ~7;
  rect_out[1] = non_neg(kMaxX - x1);
  rect_out[2] = clamp_hi(kMaxY - y0, kMaxY) | 7;
  rect_out[3] = clamp_hi(kMaxX - x0, kMaxX);
}

// Native reimplementation of get_current_temperature (0x468a4): mutex-reads
// the library's own cached temperature global. The value is still produced
// by the library's background poll thread (FUN_0004681c: reads a hwmon
// sysfs path via fopen/strtol and subtracts a 2.0C calibration offset) -
// Phase 5 hasn't reimplemented that thread natively yet, so we read its
// output directly instead of re-polling hwmon ourselves.
static float
native_get_current_temperature() {
  pthread_mutex_t* mutex = resolve_ptr<pthread_mutex_t*>(0x6d180);
  float* cached_temp = resolve_ptr<float*>(0x66e20);
  pthread_mutex_lock(mutex);
  float temp = *cached_temp;
  pthread_mutex_unlock(mutex);
  return temp;
}

void
swtcon_lock() {
#if NATIVE_UPDATE
  // LockSwapMutex: take the update-queue mutex (g_dwUpdateQueueMutex @0x6709c).
  pthread_mutex_lock(resolve_ptr<pthread_mutex_t*>(0x6709c));
#else
  qsgepaper_lock();
#endif
}

void
swtcon_update(update_data* data) {
#if NATIVE_UPDATE
  // Library leaf routines (see banner above).
  auto fn_dispatch = resolve_ptr<void (*)(void*, void*, void*)>(0x4fff8);
  auto fn_select = resolve_ptr<void* (*)(float, void*, void*, unsigned)>(0x4535c);
  auto fn_valid = resolve_ptr<int (*)(void*)>(0x409e4);
  auto fn_subtract = resolve_ptr<void (*)(void*, void*)>(0x3be10);
  auto fn_hook = resolve_ptr<void (*)(void*, void*)>(0x1ec58);

  void** g_pDataBuffer = resolve_ptr<void**>(0x670bc);
  void** g_pBackBuffer = resolve_ptr<void**>(0x670c0);
  void* g_waveform = resolve_ptr<void*>(0x67080);
  void** accum = resolve_ptr<void**>(0x670c4); // accumulation list head
  int* accum_count = resolve_ptr<int*>(0x670cc);
  void** incoming = resolve_ptr<void**>(0x67090); // batch list head

  alignas(16) unsigned char item[0x5c];
  char* I = (char*)item;
  native_update_item_ctor(item); // sets seq id at +0x1c, empties the shared_ptrs / list

  // The input rect {y,x,height,width} is byte-reversed per 64-bit lane
  // (vrev64.32) to {x,y,width,height} before clamping.
  int rev[4] = { data->x, data->y, data->width, data->height };
  int rect[4];
  native_clamp_update_rect(rect, rev);
  memcpy(I + 0xc, rect, sizeof(rect));

  int seq = *(int*)(I + 0x1c); // return value (unused by callers today)

  // Reject empty/degenerate rectangles (x1>=x0 && y0<=y1).
  bool ok = !(rect[2] < rect[0]) && !(rect[1] > rect[3]);

  if (ok) {
    int flags = data->flags;
    *(int*)(I + 0x58) = data->pixel_mode;
    *(short*)(I + 0x34) = (short)data->update_mode;
    *(uint8_t*)(I + 0x54) = (uint8_t)(flags & 1);        // Sync
    *(uint8_t*)(I + 0x55) = (uint8_t)((flags >> 1) & 1); // FullRefresh
    float temp = native_get_current_temperature();
    if (flags & 8) // FastDraw: caller supplies an explicit temperature
      temp = (float)data->zero;
    *(float*)(I + 0x38) = temp;

    fn_dispatch(item, *g_pDataBuffer, *g_pBackBuffer);

    // Select the LUT and move the returned shared_ptr into the item (+0x2c),
    // releasing whatever was there (empty after the ctor).
    void* out_sp[2] = { nullptr, nullptr };
    fn_select(temp, out_sp, g_waveform, (unsigned)(int)*(short*)(I + 0x34));
    void* old_ctrl = *(void**)(I + 0x30);
    *(void**)(I + 0x2c) = out_sp[0];
    *(void**)(I + 0x30) = out_sp[1];
    release_sp(old_ctrl);

    if (fn_valid(I + 0x2c)) {
      int* lut = *(int**)(I + 0x2c);
      *(short*)(I + 0x2a) = (short)(lut[0] - 1); // packed LUT width - 1

      // Clip this rectangle out of the pending accumulation regions and out of
      // every not-yet-locked batch already queued for the worker.
      fn_subtract(accum, item);
      for (void* b = incoming[0]; b != (void*)incoming; b = *(void**)b) {
        if (*((uint8_t*)b + 0x15) == 0)
          fn_subtract((char*)b + 8, item);
      }

      // Enqueue: 100-byte list node, deep-copy the item into node+8, hook it at
      // the tail of the accumulation list, bump the count.
      void* node = ::operator new(100);
      native_update_item_copy((char*)node + 8, item);
      fn_hook(node, accum);
      (*accum_count)++;
    } else {
      seq = -1;
    }
  } else {
    seq = -1;
  }

  // Work-item destructor (inlined tail of queue_update): free the embedded
  // std::list<int> at +0x48, then release the three shared_ptrs.
  void** sub = (void**)(I + 0x48);
  for (void* n = sub[0]; n != (void*)sub;) {
    void* nx = *(void**)n;
    ::operator delete(n);
    n = nx;
  }
  release_sp(*(void**)(I + 0x40)); // +0x3c shared_ptr
  release_sp(*(void**)(I + 0x30)); // +0x2c shared_ptr (LUT)
  release_sp(*(void**)(I + 0x04)); // +0x00 shared_ptr
  (void)seq;
#else
  qsgepaper_update(data);
#endif
}

void
swtcon_unlock_post() {
#if NATIVE_UPDATE
  void** incoming = resolve_ptr<void**>(0x67090);  // batch list head
  void** accum = resolve_ptr<void**>(0x670c4);     // accumulation list head
  int* accum_count = resolve_ptr<int*>(0x670cc);
  short* accum_flag = resolve_ptr<short*>(0x670d0);
  pthread_mutex_t* mutex = resolve_ptr<pthread_mutex_t*>(0x6709c);
  sem_t* sem = resolve_ptr<sem_t*>(0x67068);

  auto fn_batch = resolve_ptr<void* (*)(void*, void*, void*)>(0x3ea98);

  // Find the insertion point: walk backwards from the head, skipping batches
  // already claimed by the worker (flag @+0x15 set), so this batch is queued
  // after them but before any pending ones.
  void* first = incoming[0];
  void* pos = (void*)incoming;
  while (pos != first) {
    void* prev = ((void**)pos)[1];
    if (*((uint8_t*)prev + 0x15) == 0)
      break;
    pos = prev;
  }

  // Clone the accumulated regions into a fresh batch node hooked before `pos`,
  // then free the originals and reset the accumulation list to empty.
  fn_batch((void*)incoming, pos, (void*)accum);
  native_free_update_region_list((void*)accum);
  accum[0] = (void*)accum;
  accum[1] = (void*)accum;
  *accum_count = 0;
  *accum_flag = 0;

  pthread_mutex_unlock(mutex);
  sem_post(sem); // wake the display thread
#else
  qsgepaper_unlock_post();
#endif
}

void
swtcon_wait() {
#if NATIVE_UPDATE
  // WaitForUpdate: spin until shutdown or the batch queue drains.
  int* g_nShutdownRequested = resolve_ptr<int*>(0x6708c);
  void** incoming = resolve_ptr<void**>(0x67090);
  while (*g_nShutdownRequested == 0 && incoming[0] != (void*)incoming) {
    usleep(100);
  }
#else
  qsgepaper_wait();
#endif
}

// Walk g_waveform_struct (a std::vector<ModeEntry*> at 0x67080) and print each
// LUT's metadata plus a checksum of its packed data. The ModeEntry/LUTEntry
// layout matches the library (ABI=1), so this works whether the struct was
// populated by native_load_waveform or the library's load_waveform, letting us
// A/B the two byte-for-byte.
void
swtcon_dump_waveform() {
  auto* vec = (std::vector<ModeEntry*>*)resolve_ptr<void*>(0x67080);
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
  struct {
    const char* name;
    uintptr_t ptr_addr;
    size_t len;
  } bufs[] = {
    { "LUT", 0x6d350, 0x165800 },
    { "gamma", 0x6d1d4, 0x4400 },
    { "statebuffer", 0x6d1d0, 0x503580 },
  };
  for (auto& b : bufs) {
    void* p = *resolve_ptr<void**>(b.ptr_addr);
    uint32_t sum = 2166136261u;
    if (p) {
      uint8_t* d = (uint8_t*)p;
      for (size_t i = 0; i < b.len; i++)
        sum = (sum ^ d[i]) * 16777619u;
    }
    std::cout << "buf " << b.name << " ptr=" << p << " len=" << b.len
              << " fnv=0x" << std::hex << sum << std::dec << std::endl;
    // Also write the raw bytes to /tmp for byte-level A/B (cmp) between native
    // and library init.
    if (p) {
      std::string path = std::string("/tmp/dump_") + b.name + ".bin";
      FILE* f = fopen(path.c_str(), "wb");
      if (f) {
        fwrite(p, 1, b.len, f);
        fclose(f);
      }
    }
  }
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
