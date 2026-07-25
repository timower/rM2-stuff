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

// --- Update queue / worker+display thread state -----------------------
// Fully contiguous from g_list_processed_updates (0x66fd8) through the
// accumulation list's flag (0x670d0). Written by swtcon_init (thread
// startup), read/written by swtcon_update/lock/unlock_post/wait, and read
// by swtcon_shutdown (drain + join).
struct UpdateQueueGlobals {
  ListHead listProcessedUpdates;             // 0x66fd8
  uint8_t _reserved_0x66fe0[0x66fec - 0x66fe0];
  pthread_mutex_t workerCondMutex;           // 0x66fec
  uint8_t _reserved_0x67004[0x67008 - 0x67004];
  pthread_cond_t workerCond;                  // 0x67008
  uint8_t _reserved_0x67038[0x6703c - 0x67038];
  pthread_mutex_t displayTimingMutex;         // 0x6703c
  uint8_t _reserved_0x67054[0x67068 - 0x67054];
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
QQ_ASSERT(workerCondMutex, 0x66fec);
QQ_ASSERT(workerCond, 0x67008);
QQ_ASSERT(displayTimingMutex, 0x6703c);
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
  return resolve_ptr<UpdateQueueGlobals*>(kUpdateQueueGlobalsAddr);
}

// --- Standalone globals (isolated - no neighbor close enough to bridge
// with a confidently-sized gap) ------------------------------------------
constexpr uintptr_t kPidFdAddr = 0x66dec;               // int, g_nPidFd
constexpr uintptr_t kCachedTemperatureAddr = 0x66e20;   // float, g_flCachedTemperature
constexpr uintptr_t kTemperatureMutexAddr = 0x6d180;    // pthread_mutex_t, g_dwTemperatureMutex
constexpr uintptr_t kSeqCounterAddr = 0x6d178;          // int, work-item sequence id counter

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
  return resolve_ptr<StatebufferGlobals*>(kStatebufferGlobalsAddr);
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
  return resolve_ptr<FramebufferGlobals*>(kFramebufferGlobalsAddr);
}
