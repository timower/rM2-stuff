#pragma once

// Named layout of libqsgepaper.so's global state that our native code reads
// or writes directly (by address, since the library is a closed-source
// black box - see AGENTS.md for the reversing history). Historically these
// were touched via individual `resolve_ptr<T*>(0x1234)` calls scattered
// through the code with a trailing "// g_whatever" comment; this header
// gathers them into named structs so call sites read as `queue->accumCount`
// instead of a bare hex literal.
//
// Every struct below models a run of the library's .bss that we've
// confirmed is *fully contiguous* by exact address arithmetic between two
// independently-verified fields (sizes cross-checked against the real
// on-target sizeof() of pthread_mutex_t/pthread_cond_t/sem_t/pthread_t - see
// the static_asserts). Gaps between known fields are real - genuine
// unreversed library state - but their *size* is exact (next_known_addr -
// prev_addr - sizeof(prev)), not guessed, so they're safe to model as
// untouched reserved padding. Globals that are isolated (no other used
// field within a small, confidently-bridgeable distance) are kept as
// standalone named address constants instead of being forced into a
// struct with a large speculative gap.
//
// All addresses are Ghidra addresses for libqsgepaper.so 3.23.0.54/.64
// (0x10000 image base) - resolve via resolve_ptr<T*>(addr).

#include <cstddef>
#include <cstdint>
#include <linux/fb.h>
#include <pthread.h>
#include <semaphore.h>

#include "swtcon.h"

// Helper to resolve a Ghidra address to a live pointer in the loaded
// library, using the load bias swtcon_runtime_offset() computed at dlopen
// time.
template<typename T>
inline T
resolve_ptr(uintptr_t addr) {
  return (T)(swtcon_runtime_offset() + addr);
}

// A generic intrusive circular list sentinel/head: two pointers (next,
// prev), matching the layout libstdc++'s std::__detail::_List_node_base
// shares with every node hooked into one of these lists.
struct ListHead {
  void* next;
  void* prev;
};
static_assert(sizeof(ListHead) == 8, "ListHead must be two pointers");

// The library's own timestamp ABI (__clock_gettime64 - a Y2038-safe kernel
// timespec: two 64-bit fields), confirmed via disassembly of
// worker_thread_func (see AGENTS.md Phase 5) - NOT necessarily the same as
// this toolchain's own `struct timespec`, which may still be the legacy
// 32-bit-tv_sec layout. Fill by widening an ordinary clock_gettime() result
// field-by-field rather than reinterpreting/memcpy-ing a native timespec
// onto this struct.
struct Timespec64 {
  int64_t tv_sec;
  int64_t tv_nsec;
};
static_assert(sizeof(Timespec64) == 16, "Timespec64 must be two 64-bit fields");

// --- Update queue / worker+display thread state -----------------------
// Fully contiguous from g_list_processed_updates (0x66fd8) through the
// accumulation list's flag (0x670d0). Written by swtcon_init (thread
// startup), read/written by swtcon_update/lock/unlock_post/wait, and read
// by swtcon_shutdown (drain + join).
struct UpdateQueueGlobals {
  ListHead listProcessedUpdates;             // 0x66fd8
  int32_t processedUpdatesCount;             // 0x66fe0 g_nProcessedUpdatesCount
  int32_t curFrame;                          // 0x66fe4 g_nCurFrame
  int32_t targetFrame;                       // 0x66fe8 g_nTargetFrame
  pthread_mutex_t workerCondMutex;           // 0x66fec
  uint8_t _reserved_0x67004[0x67008 - 0x67004];
  pthread_cond_t workerCond;                  // 0x67008
  uint8_t flashRequested;                      // 0x67038 g_bFlashRequested (byte flag, not int32)
  uint8_t _reserved_0x67039[3];
  pthread_mutex_t displayTimingMutex;         // 0x6703c
  uint8_t _reserved_0x67054[0x67058 - 0x67054];
  Timespec64 lastPanTimestamp;                 // 0x67058 g_lastPanTimestamp - stamped alongside
                                                // every nLastPannedFrame write, under displayTimingMutex
  sem_t displayThreadSem;                      // 0x67068
  int32_t timeVar;                              // 0x67078
  int32_t workerThreadShutdown;                  // 0x6707c
  uint8_t waveformStructRaw[12];                  // 0x67080 std::vector<ModeEntry*>
  int32_t shutdownRequested;                       // 0x6708c
  ListHead listIncomingUpdates;                     // 0x67090 (BatchNode list)
  int32_t incomingBatchCount;                        // 0x67098 (unnamed in the
                                                       // library; sits right
                                                       // after the list head -
                                                       // see native_build_update_batch)
  pthread_mutex_t updateQueueMutex;                   // 0x6709c
  pthread_t displayThread;                             // 0x670b4
  pthread_t workerThread;                               // 0x670b8
  void* dataBuffer;                                      // 0x670bc g_pDataBuffer
  void* backBuffer;                                       // 0x670c0 g_pBackBuffer
  ListHead accumList;                                      // 0x670c4 (WorkItemNode list)
  int32_t accumCount;                                       // 0x670cc
  int16_t accumFlag;                                         // 0x670d0
};
constexpr uintptr_t kUpdateQueueGlobalsAddr = 0x66fd8;
#define QQ_OFFSETOF(field) (offsetof(UpdateQueueGlobals, field))
#define QQ_ASSERT(field, addr) \
  static_assert(QQ_OFFSETOF(field) == (addr) - kUpdateQueueGlobalsAddr, \
                #field " must land at " #addr)
QQ_ASSERT(listProcessedUpdates, 0x66fd8);
QQ_ASSERT(processedUpdatesCount, 0x66fe0);
QQ_ASSERT(curFrame, 0x66fe4);
QQ_ASSERT(targetFrame, 0x66fe8);
QQ_ASSERT(workerCondMutex, 0x66fec);
QQ_ASSERT(workerCond, 0x67008);
QQ_ASSERT(flashRequested, 0x67038);
QQ_ASSERT(displayTimingMutex, 0x6703c);
QQ_ASSERT(lastPanTimestamp, 0x67058);
QQ_ASSERT(displayThreadSem, 0x67068);
QQ_ASSERT(timeVar, 0x67078);
QQ_ASSERT(workerThreadShutdown, 0x6707c);
QQ_ASSERT(waveformStructRaw, 0x67080);
QQ_ASSERT(shutdownRequested, 0x6708c);
QQ_ASSERT(listIncomingUpdates, 0x67090);
QQ_ASSERT(incomingBatchCount, 0x67098);
QQ_ASSERT(updateQueueMutex, 0x6709c);
QQ_ASSERT(displayThread, 0x670b4);
QQ_ASSERT(workerThread, 0x670b8);
QQ_ASSERT(dataBuffer, 0x670bc);
QQ_ASSERT(backBuffer, 0x670c0);
QQ_ASSERT(accumList, 0x670c4);
QQ_ASSERT(accumCount, 0x670cc);
QQ_ASSERT(accumFlag, 0x670d0);
// Trailing size only has to be *at least* enough to cover every field we
// use - the compiler may round the struct's overall size up further to
// satisfy some member's (e.g. sem_t's) alignment, which is harmless since it
// only adds padding after the last field we care about.
static_assert(sizeof(UpdateQueueGlobals) >= 0x670d2 - kUpdateQueueGlobalsAddr,
              "UpdateQueueGlobals layout drift");
#undef QQ_ASSERT
#undef QQ_OFFSETOF

inline UpdateQueueGlobals*
update_queue_globals() {
  static UpdateQueueGlobals g{};
  return &g;
}

// --- Frame-pacing cursor cluster ----------------------------------------
// Contiguous, byte-verified via disassembly of worker_thread_func /
// display_thread_func (see AGENTS.md Phase 5). g_nFrameCleanupCursor is
// display_thread_func's own housekeeping cursor (untouched by the worker
// thread); bWorkerThreadBusy/nLastPannedFrame are written by the worker
// thread and read cross-thread by the display thread's frame-pacing logic.
struct FrameCursorGlobals {
  int32_t nFrameCleanupCursor;  // 0x66dd4
  uint8_t bWorkerThreadBusy;    // 0x66dd8 (byte flag, not int32)
  uint8_t _pad[3];
  int32_t nLastPannedFrame;     // 0x66ddc
};
constexpr uintptr_t kFrameCursorGlobalsAddr = 0x66dd4;
static_assert(offsetof(FrameCursorGlobals, bWorkerThreadBusy) == 0x66dd8 - kFrameCursorGlobalsAddr, "");
static_assert(offsetof(FrameCursorGlobals, nLastPannedFrame) == 0x66ddc - kFrameCursorGlobalsAddr, "");
static_assert(sizeof(FrameCursorGlobals) >= 0x66de0 - kFrameCursorGlobalsAddr,
              "FrameCursorGlobals layout drift");

inline FrameCursorGlobals*
frame_cursor_globals() {
  static FrameCursorGlobals g{};
  return &g;
}

// --- Standalone globals (isolated - no neighbor close enough to bridge
// with a confidently-sized gap) ------------------------------------------
// Formerly resolve_ptr<T*>(fixed_addr) into the library's own .bss/.data
// (g_nPidFd 0x66dec, g_flCachedTemperature 0x66e20, g_dwTemperatureMutex
// 0x6d180, the work-item sequence id counter 0x6d178) - now natively-owned
// storage (Phase 7). g_nPidFd's slot is unused: native_create_pid_file
// (native_init.cpp) already writes the real fd into its own native
// g_nPidFdNative global instead, so native_unlock_pid_file reads that
// directly rather than through here.

// NOT zero-initialized in the real library: g_flCachedTemperature's initial
// bytes in libqsgepaper.so's own data image are 0x41c80000 (25.0f), a
// genuine initialized default (room temperature), not .bss. Confirmed via
// Ghidra memory inspection at 0x66e20 after tracking down a real behavioral
// divergence this got wrong - with 0.0f here, a sensor-read failure (which
// happens on the emulator, no hwmon path) leaves the two sides selecting
// different temperature-indexed waveform LUT entries (0C vs 25C bucket),
// visible as a completely different phase count/content for the startup
// flash waveform (149 phases vs ~99) even though everything else about the
// flash sequence matches exactly.
inline float*
cached_temperature_ptr() {
  static float t = 25.0f;
  return &t;
}

// Zero-initialized, matching the library's own .bss state before
// swtcon_init explicitly pthread_mutex_init's it (see swtcon.cpp).
inline pthread_mutex_t*
temperature_mutex() {
  static pthread_mutex_t m{};
  return &m;
}

inline int*
seq_counter_ptr() {
  static int c = 0;
  return &c;
}

// backBuffer dirty-gate array (formerly resolve_ptr<uint8_t*>(0x670d8) into
// the library's .bss, byte-verified via decompilation of both
// display_thread_func and advance_work_item_frames - see
// swtcon_architecture.md §6.2 step 1 / §6.4): 16 buckets, one per
// frame-slot ring position, each SCREEN_WIDTH (0x57c) bytes, one byte per
// column. advance_work_item_frames marks a newly-rendered frame's
// [sp3.x0, sp3.x1] columns dirty in its ring-position's bucket;
// display_thread_func's stale-row cleanup drains + zeroes each bucket once
// its ring position falls out of the live window.
constexpr int32_t kDirtyGateRowBytes = 0x57c; // SCREEN_WIDTH
constexpr int32_t kDirtyGateBucketCount = 16;

// native_stale_row_cleanup (native_display.cpp) computes `bucket = i % 16`
// for `i` as low as nFrameCleanupCursor-15, which is negative during the
// first ~15 display-thread ticks after startup (C++ `%` keeps the sign of
// the dividend) - a real quirk in the original library, transcribed
// verbatim rather than "fixed" (see swtcon_architecture.md §8). Against the
// library's own .bss this backward reach landed harmlessly on whatever
// else was there (effectively zero this early); a bare native array has no
// such margin behind it, so pad kDirtyGateBucketCount always-zero buckets
// before the real array and hand out a pointer into the middle - the
// padding is never legitimately written (every write-side index is a
// non-negative frame number mod 16), so it stays zero forever, exactly
// reproducing the observed-harmless behavior instead of segfaulting on an
// unmapped page.
inline uint8_t*
backbuffer_dirty_gate() {
  static uint8_t g[2 * kDirtyGateBucketCount * kDirtyGateRowBytes]{};
  return g + (size_t)kDirtyGateBucketCount * kDirtyGateRowBytes;
}

// --- Persisted statebuffer + gamma table --------------------------------
// Fully contiguous, all three pointers/size wired together at init
// (swtcon_init) and torn down together (native_free_statebuffer).
struct StatebufferGlobals {
  void* pStatebuffer;   // 0x6d1d0
  void* pGammaTable;    // 0x6d1d4
  int32_t nSize;         // 0x6d1d8
};
constexpr uintptr_t kStatebufferGlobalsAddr = 0x6d1d0;
static_assert(offsetof(StatebufferGlobals, pGammaTable) == 0x6d1d4 - kStatebufferGlobalsAddr, "");
static_assert(offsetof(StatebufferGlobals, nSize) == 0x6d1d8 - kStatebufferGlobalsAddr, "");
static_assert(sizeof(StatebufferGlobals) == 0xc, "StatebufferGlobals layout drift");

inline StatebufferGlobals*
statebuffer_globals() {
  static StatebufferGlobals g{};
  return &g;
}

// --- Framebuffer + LUT state ---------------------------------------------
// Fully contiguous except one confirmed 4-byte unreversed gap between pLUT
// and nFbFd (its size is exact: nFbFd's address is independently known, so
// the gap is pinned even though its contents aren't).
struct FramebufferGlobals {
  void* pLUT;                       // 0x6d350
  uint8_t _reserved_0x6d354[4];
  int32_t nFbFd;                     // 0x6d358
  struct fb_fix_screeninfo fbFix;     // 0x6d35c
  struct fb_var_screeninfo fbVar;      // 0x6d3a0
  int32_t nFbSizeX;                     // 0x6d440 bytes per frame slot
  int32_t nFbSizeY;                      // 0x6d444 number of frame slots
  int32_t nIsFbBlanked;                   // 0x6d448
  void* pFbMmap;                           // 0x6d44c
};
constexpr uintptr_t kFramebufferGlobalsAddr = 0x6d350;
#define FB_OFFSETOF(field) (offsetof(FramebufferGlobals, field))
#define FB_ASSERT(field, addr) \
  static_assert(FB_OFFSETOF(field) == (addr) - kFramebufferGlobalsAddr, \
                #field " must land at " #addr)
FB_ASSERT(nFbFd, 0x6d358);
FB_ASSERT(fbFix, 0x6d35c);
FB_ASSERT(fbVar, 0x6d3a0);
FB_ASSERT(nFbSizeX, 0x6d440);
FB_ASSERT(nFbSizeY, 0x6d444);
FB_ASSERT(nIsFbBlanked, 0x6d448);
FB_ASSERT(pFbMmap, 0x6d44c);
static_assert(sizeof(FramebufferGlobals) == 0x6d450 - kFramebufferGlobalsAddr,
              "FramebufferGlobals layout drift");
#undef FB_ASSERT
#undef FB_OFFSETOF

inline FramebufferGlobals*
framebuffer_globals() {
  static FramebufferGlobals g{};
  return &g;
}
