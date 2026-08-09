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
#include <vector>

#include "swtcon.h"

// Structs below (here and in update.h) pin offsets/sizes to the real 32-bit
// ARM library's layout via static_assert - meaningless on a 64-bit host, so
// those checks are guarded by this rather than failing every dev-host build.
#define SWTCON_32BIT_ABI_CHECK (UINTPTR_MAX == 0xFFFFFFFFu)

// Forward-declared rather than #include "init.h" (which defines
// ModeEntry) to avoid a header cycle - init.h itself includes this
// file. A vector of pointers only needs the pointee declared, not complete.
struct ModeEntry;

// The panel's native resolution - every full-screen buffer/rect bound in
// this codebase derives from these two. Shared here (rather than each
// production file keeping its own copy) since update.cpp and
// display.cpp both need it and drift between copies would be a real
// correctness risk, not just cosmetic - see e.g. clamp_update_rect's
// reflection formula. The handful of standalone black-box probe/bench tools
// (playback_kernel_bench.cpp etc.) intentionally keep their own local copy
// instead, matching their own independence from the production headers.
constexpr int32_t kScreenWidth = 1404;
constexpr int32_t kScreenHeight = 1872;

// The number of real, panel-visible frame slots in the ring buffer worker/
// display-thread frame-cursor arithmetic wraps around (curFrame % N,
// per-tick dirty-gate bucketing, etc.) - see FbInitParams::frameCount in
// swtcon.cpp, which configures the framebuffer with exactly this many slots
// plus one extra (kInitFrameSlotIndex below) reserved for priming/flashing.
constexpr int32_t kFrameSlotRingCount = 16;

// The one extra frame slot beyond the kFrameSlotRingCount real ones (index
// == kFrameSlotRingCount since slots are 0-based) - used only to prime the
// panel controller (prime_display) and during the startup flash
// sequence, never part of the normal playback ring.
constexpr int32_t kInitFrameSlotIndex = kFrameSlotRingCount;

// The panel's own per-frame-tick pacing period, in microseconds - sizes
// display_thread_func's frame-pacing target formula (display.cpp)
// and is the only concrete real-time deadline in this codebase to check
// kernel/dispatch performance against (see playback_kernel_bench.cpp).
constexpr double kPanelFrameTickUs = 11761.0;

// display_thread_func's pacing-target formula (display.cpp) scales
// the batch's leftmost column by this factor (then /1000) to get a target
// elapsed-time budget - an empirically reversed constant (byte-verified
// against the real disassembly, not derived from any other constant here)
// with no more specific name available than what it computes.
constexpr int64_t kColumnPaceUsPerColumnX1000 = 0x1d96;

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
#if SWTCON_32BIT_ABI_CHECK
static_assert(sizeof(ListHead) == 8, "ListHead must be two pointers");
#endif

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
// UpdateQueueGlobals and update_queue_globals() moved to update.h
// (Step 4 of the shared_ptr/list cleanup) - three of its fields
// (listProcessedUpdates/accumList/listIncomingUpdates) are now real
// std::list<WorkItem>/std::list<Batch>, which need WorkItem/Batch complete
// wherever the struct's implicit destructor gets instantiated (i.e.
// wherever update_queue_globals()'s function body, defining the static
// local instance, is compiled) - and WorkItem/Batch are only defined in
// update.h, included by every file that actually calls
// update_queue_globals() (swtcon.cpp, update.cpp, display.cpp)
// but not by init.h/.cpp, which never needed this struct.

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

// Formerly resolve_ptr<T*>(fixed_addr) into the library's own .bss/.data
// (g_nPidFd 0x66dec, g_flCachedTemperature 0x66e20, g_dwTemperatureMutex
// 0x6d180, the work-item sequence id counter 0x6d178) - now natively-owned
// storage, held (along with every other module-level global in this
// codebase) in SwtconState - see update.h for the struct itself and every
// accessor declared below.

// backBuffer dirty-gate array (formerly resolve_ptr<uint8_t*>(0x670d8) into
// the library's .bss, byte-verified via decompilation of both
// display_thread_func and advance_work_item_frames - see
// swtcon_architecture.md §6.2 step 1 / §6.4): one bucket per frame-slot ring
// position (kFrameSlotRingCount), each kScreenWidth bytes, one byte per
// column. advance_work_item_frames marks a newly-rendered frame's
// [pixelTransitions.x0, pixelTransitions.x1] columns dirty in its
// ring-position's bucket;
// display_thread_func's stale-row cleanup drains + zeroes each bucket once
// its ring position falls out of the live window.
constexpr int32_t kDirtyGateRowBytes = kScreenWidth;
constexpr int32_t kDirtyGateBucketCount = kFrameSlotRingCount;

// stale_row_cleanup (display.cpp) computes `bucket = i % 16`
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

// --- Persisted statebuffer + gamma table --------------------------------
// Fully contiguous, all three pointers/size wired together at init
// (swtcon_init) and torn down together (free_statebuffer).
struct StatebufferGlobals {
  void* pStatebuffer;   // 0x6d1d0
  void* pGammaTable;    // 0x6d1d4
  int32_t nSize;         // 0x6d1d8
};
constexpr uintptr_t kStatebufferGlobalsAddr = 0x6d1d0;
#if SWTCON_32BIT_ABI_CHECK
static_assert(offsetof(StatebufferGlobals, pGammaTable) == 0x6d1d4 - kStatebufferGlobalsAddr, "");
static_assert(offsetof(StatebufferGlobals, nSize) == 0x6d1d8 - kStatebufferGlobalsAddr, "");
static_assert(sizeof(StatebufferGlobals) == 0xc, "StatebufferGlobals layout drift");
#endif

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
#if SWTCON_32BIT_ABI_CHECK
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
#endif

// Accessors for FrameCursorGlobals/StatebufferGlobals/FramebufferGlobals
// above, plus cached_temperature_ptr()/temperature_mutex()/seq_counter_ptr()/
// backbuffer_dirty_gate() - all defined in update.h alongside SwtconState
// (which embeds these structs as members), not here: they need SwtconState
// complete, and SwtconState needs WorkItem/Batch complete, both of which are
// only available once update.h itself has been included.
FrameCursorGlobals* frame_cursor_globals();
StatebufferGlobals* statebuffer_globals();
FramebufferGlobals* framebuffer_globals();
float* cached_temperature_ptr();
pthread_mutex_t* temperature_mutex();
int* seq_counter_ptr();
uint8_t* backbuffer_dirty_gate();
