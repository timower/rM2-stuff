#pragma once

// Native reimplementation of swtcon_update/lock/unlock_post/wait - see
// AGENTS.md "Phase 4/4b" for the reversing history. This header holds named
// struct layouts for the wire formats the library itself uses (work items,
// their containing list nodes, the region-rows buffer
// dispatch_update_regions produces, and update-batch nodes), replacing what
// used to be raw `+0xNN` byte-offset arithmetic through the file.
//
// Every struct is designed to be naturally self-aligning (each field already
// lands on the byte offset it needs given the fields before it), so the
// static_asserts below are a safety net, not evidence the layout only works
// because of implicit compiler padding - there isn't any.

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <vector>

#include "init.h" // ModeEntry
#include "qsgepaper_globals.h"
#include "swtcon.h"

// Builds a non-owning std::shared_ptr<T> (get()==ptr, use_count()==0, no
// control block) aliasing externally-owned storage - the shared_ptr
// equivalent of the old SpRef{ptr, ctrl=nullptr} pattern the probe/bench
// tools use to point a WorkItem's regionRows/lut/pixelTransitions at their own
// stack/scratch test buffers without giving the shared_ptr any
// ownership/refcounting responsibility over them (those buffers are often
// guard-paged and must not be freed via this alias). Standard-sanctioned:
// the aliasing constructor with an empty owning shared_ptr always produces
// {ptr, ctrl=nullptr}.
template<typename T>
inline std::shared_ptr<T>
non_owning_sp(T* ptr) {
  return std::shared_ptr<T>(std::shared_ptr<void>(), ptr);
}

// The render kernel's "gated/inactive" pixel sentinel (native_render_kernel_
// formula's case 7, update.cpp): the byte value render_update_kernel
// writes into RegionRows::dataPtr for a pixel the backBuffer says is
// inactive. commit_item later reads that same buffer back and
// checks for this exact value to decide a pixel produced no real change -
// the two checks must agree on this sentinel or a legitimately-computed
// pixel value could be mistaken for "nothing happened here."
constexpr uint16_t kGatedPixelSentinel = 0x20;

// commit_item's packed-transition "unchanged" marker: written into
// pixelTransitions instead of the real (oldState<<5)|newState packing
// whenever a pixel didn't actually change (or was gated - see
// kGatedPixelSentinel above). 0x0400 can never collide with a real packed
// value since bit 10 is otherwise always 0 (state values are <=0x1f, and
// 0x1f<<5 = 0x3e0, one bit short of bit 10).
constexpr uint16_t kPixelTransitionUnchanged = 0x0400;

// The bit width of one packed pixel state - see kPixelTransitionUnchanged's
// packing formula, (oldState<<5)|newState, in commit_item
// (display.cpp). NOT used to unpack: playback_kernel_intrinsics.cpp's LUT
// index reads the raw packed u16 directly (see its own comment for why that
// still lines up with this packing - only because mode_width==32 there).
constexpr int kPixelTransitionShift = 5;

// A work-item rect in the library's native {y0,x0,y1,x1} field order (see
// WorkItem below).
struct Rect {
  int32_t y0, x0, y1, x1;
};

// The pre-clamp rect order swtcon_update builds from update_data's
// x0/y0/x1/y1 (see clamp_update_rect's comment for why this
// isn't the same axis order as Rect).
struct XYRect {
  int32_t x0, y0, x1, y1;
};

// dispatch_update_regions's (0x4fff8) output, hung off a work item's
// `regionRows` shared_ptr - the raw pointer half points directly at this
// struct. Column-major: address(y,x) = dataPtr + stride*(x-x0) + (y-y0).
//
// `dataPtr` is a plain non-owning pointer - deliberately not a smart pointer
// or an owning destructor here, since probe/bench tools construct bare
// RegionRows values on the stack aliasing externally-owned (often
// guard-paged) test buffers they must NOT have freed out from under them at
// scope exit. The two real allocation sites that DO own their buffer
// (dispatch_update_regions's regionRows, commit_item's
// pixelTransitions, which stores uint16_t payload despite the uint8_t* type
// - see its comment)
// free it via a custom deleter on the owning shared_ptr instead.
struct RegionRows {
  uint8_t* dataPtr;  // size = stride * (x1-x0+1)
  int32_t y0, x0, y1, x1;
  int32_t stride;  // round_up(y1-y0+1, 16)
  int32_t size;
};
#if SWTCON_32BIT_ABI_CHECK
static_assert(sizeof(RegionRows) == 0x1c, "RegionRows layout drift");
#endif

// The row-count alignment every RegionRows-shaped buffer's `stride` is
// rounded up to (dispatch_update_regions's regionRows,
// commit_item's pixelTransitions in display.cpp) - both
// compute `round_up(rows, kRegionRowsStrideAlign)` independently, so this is
// the one place that idiom should be named rather than re-spelled as a bare
// `+0xf) & ~0xf` at each site.
constexpr int32_t kRegionRowsStrideAlign = 16;

// The library's internal "update work item", 0x5c bytes. Mirrors
// update_item_ctor (0x3ffd0). Lives embedded at +8 inside a WorkItemNode.
struct WorkItem {
  std::shared_ptr<RegionRows> regionRows; // +0x00 (dispatch_update_regions's output)
  // +0x08 cached RegionRows::dataPtr (the newly-rendered per-pixel byte),
  // rebased to this item's rect origin (see piece_builder). Stored as
  // uintptr_t rather than a typed pointer, to match the real library's ABI -
  // dispatch_processed_regions_probe.cpp still calls the real by-address
  // dispatch_processed_regions, whose commit kernels (FUN_0004f8f0/
  // FUN_0004e680) read this field via WorkItem+0x08 (see commit_item's
  // comment). uintptr_t (not int32_t) so the round-trip through this field
  // is lossless on a 64-bit host too: on the real 32-bit ARM target
  // uintptr_t is exactly 4 bytes, identical to int32_t (see WI_ASSERT's
  // static_assert below), but on a 64-bit dev-host build an int32_t can't
  // represent a real heap address at all (glibc heap addresses commonly sit
  // around 0x55...), which silently corrupted this field's round-trip
  // through commit_item (display.cpp) - a real, reachable crash through the
  // normal swtcon_update -> [worker/display threads] -> commit_item
  // pipeline on this platform before this field widened.
  uintptr_t pixelDataPtr;
  int32_t rectY0;               // +0x0c
  int32_t rectX0;                // +0x10
  int32_t rectY1;                 // +0x14
  int32_t rectX1;                  // +0x18
  int32_t seqId;                    // +0x1c
  int32_t frameCursor;               // +0x20 [confirmed] next display-frame index this item's waveform should render into (advance_work_item_frames/FUN_0003ec78)
  int32_t frameAnchor;                // +0x24 [confirmed] frame this item's playback started on; frameAnchor+lutWidthMinus1 = last active frame
  int16_t phase;                       // +0x28 [confirmed] frames of this item's waveform already rendered; lutWidthMinus1-phase = frames remaining
  int16_t lutWidthMinus1;               // +0x2a
  std::shared_ptr<LUTEntry> lut;          // +0x2c (selected waveform LUT)
  int16_t mode;                           // +0x34
  int16_t _pad0x36;
  float temperature;                       // +0x38
  // +0x3c [confirmed] a second per-pixel buffer distinct from regionRows,
  // holding a packed uint16_t "(oldState<<5)|newState" transition value per
  // pixel (0x0400 as the "unchanged" sentinel - see commit_item),
  // NOT a snapshot of screen state itself (that's the unrelated global
  // g_pStateBufferNative/`state`, indexed screen-wide by column/row, not by
  // item - don't confuse the two). advance_work_item_frames reads
  // pixelTransitions->x0/x1 directly (RegionRows' own offsets) to mark the
  // backBuffer dirty-gate array, and the still-library display-commit
  // kernels (FUN_0004f8f0/FUN_0004e680, §6.3) write into it.
  std::shared_ptr<RegionRows> pixelTransitions;
  // +0x44 [confirmed] the u16 pixelTransitions buffer REBASED to the item's
  // (post-narrowing) rect origin: pixelTransitions->dataPtr +
  // stride*(rectX0-pixelTransitions.x0) + (rectY0-pixelTransitions.y0), the
  // same rebasing as `pixelDataPtr` vs regionRows. The still-library
  // playback kernels FUN_0004a140/FUN_0004a234 read/write per-pixel
  // transitions through THIS pointer (item+0x44), NOT via
  // pixelTransitions->dataPtr, and index it with narrowed-rect-relative
  // coords - so it must be set (and rebased) whenever pixelTransitions is
  // (re)allocated. Setting it to the un-rebased buffer base leaves valid
  // memory (no SIGSEGV) but reads the wrong offset for any narrowed/split
  // item -> edge artifacts (commit_item, empirically A/B-verified
  // via SWTCON_LIBDISPATCH). Leaving it stale/null -> SIGSEGV
  void* transitionDataPtr;
  // +0x48, was originally the library's embedded std::list<int> (despite the
  // "int", each element actually held a raw WorkItem* pointing at another
  // in-flight item this one's rendering depends on - confirmed via
  // disassembly of build_overlap_dependency_list, 0x3a838). Nothing by-address
  // ever reads/writes this field (unlike regionRows/lut/pixelTransitions/
  // transitionDataPtr) - it's built and consumed entirely by native code
  // (advance_work_item_frames,
  // build_overlap_dependency_list, gc_processed_updates) - so a
  // plain std::vector<WorkItem*> of non-owning back-pointers is both simpler
  // and, unlike the original std::list<int> mimicry, doesn't need to match
  // any real ABI at all.
  std::vector<WorkItem*> deps;                // +0x48
  uint8_t sync;                                 // +0x54
  uint8_t fastDraw;                              // +0x55 (update_data's FastDraw flag - see swtcon.h)
  uint8_t _pad0x56[2];
  int32_t pixelMode;                              // +0x58
};
#if SWTCON_32BIT_ABI_CHECK
#define WI_ASSERT(field, off) \
  static_assert(offsetof(WorkItem, field) == (off), #field " must land at " #off)
WI_ASSERT(pixelDataPtr, 0x08);
WI_ASSERT(rectY0, 0x0c);
WI_ASSERT(rectX0, 0x10);
WI_ASSERT(rectY1, 0x14);
WI_ASSERT(rectX1, 0x18);
WI_ASSERT(seqId, 0x1c);
WI_ASSERT(frameCursor, 0x20);
WI_ASSERT(frameAnchor, 0x24);
WI_ASSERT(phase, 0x28);
WI_ASSERT(lutWidthMinus1, 0x2a);
WI_ASSERT(lut, 0x2c);
WI_ASSERT(mode, 0x34);
WI_ASSERT(temperature, 0x38);
WI_ASSERT(pixelTransitions, 0x3c);
WI_ASSERT(transitionDataPtr, 0x44);
WI_ASSERT(deps, 0x48);
WI_ASSERT(sync, 0x54);
WI_ASSERT(fastDraw, 0x55);
WI_ASSERT(pixelMode, 0x58);
#undef WI_ASSERT
static_assert(sizeof(WorkItem) == 0x5c, "WorkItem layout drift");
// std::shared_ptr<T> is always exactly {T* ptr; _Sp_counted_base* ctrl} (two
// pointers) regardless of T - verified against this project's own toolchain
// libstdc++ headers (bits/shared_ptr_base.h) - so swapping in real
// shared_ptr fields here doesn't change any offset above. Guards against a
// future libstdc++ ABI change silently growing WorkItem.
static_assert(sizeof(std::shared_ptr<RegionRows>) == 8, "shared_ptr<T> must be two pointers");
static_assert(sizeof(std::shared_ptr<LUTEntry>) == 8, "shared_ptr<T> must be two pointers");
// deps's offset/size only need to hold sync/fastDraw/pixelMode at their
// asserted offsets above - no real ABI to match (see deps's own comment) -
// but pin it anyway as a regression guard against a future libstdc++ vector
// layout change.
static_assert(sizeof(std::vector<WorkItem*>) == 0xc, "std::vector<WorkItem*> must be 3 pointers (12 bytes) on this 32-bit ARM target");
#endif // SWTCON_32BIT_ABI_CHECK

// A work item's containing intrusive-list node - 100 bytes total, matching
// every `operator_new(100)` / node+8 pattern in the library.
struct WorkItemNode {
  WorkItemNode* next;
  WorkItemNode* prev;
  WorkItem item;
};
#if SWTCON_32BIT_ABI_CHECK
static_assert(sizeof(WorkItemNode) == 100, "WorkItemNode must match the library's 100-byte list node");
#endif

// A claimed batch of work items (build_update_batch's output, 0x18 bytes),
// hooked into the incoming-updates list. `mode` and the "claimed by worker"
// flag the still-library worker_thread_func reads share the same +0x14
// 16-bit slot but the exact boundary between them (does the worker write
// the whole short, or just its high byte at +0x15?) is unverified, so the
// claimed-flag read stays a raw offset access (see BatchNodeClaimed) rather
// than a named field here.
struct BatchNode {
  BatchNode* next;
  BatchNode* prev;
  ListHead subList;  // +0x08, head of a WorkItemNode circular list
  int32_t count;      // +0x10
  int16_t mode;        // +0x14
};
#if SWTCON_32BIT_ABI_CHECK
static_assert(offsetof(BatchNode, subList) == 0x08, "");
static_assert(offsetof(BatchNode, count) == 0x10, "");
static_assert(offsetof(BatchNode, mode) == 0x14, "");
static_assert(sizeof(BatchNode) == 0x18, "BatchNode layout drift");
#endif

// True if the worker thread has already claimed this batch (won't be
// touched again by swtcon_update/unlock_post's subtract/enqueue paths).
// BatchNode/this function are kept only for the probe tools (see below) -
// production code uses Batch::claimed instead (see its comment).
inline bool
BatchNodeClaimed(const BatchNode* b) {
  return *((const uint8_t*)b + 0x15) != 0;
}

// The production equivalent of a claimed batch of work items
// (build_update_batch's output) - BatchNode above is kept only for the
// probe tools, which call the real by-address library functions directly
// and need that exact byte layout; this codebase's own queues have no such
// ABI constraint (see UpdateQueueGlobals's comment) so they use a real
// std::list<WorkItem> instead of a hand-rolled intrusive list.
struct Batch {
  std::list<WorkItem> subList;
  int16_t mode = 0;
  // Mirrors BatchNodeClaimed's raw +0x15 byte read. Confirmed via grep that
  // nothing in the native pipeline ever sets this - the still-library
  // worker_thread_func's own batch-claiming logic was never ported/
  // reversed, so this always reads false in practice. Kept as a named field
  // purely to preserve that already-inert check's current behavior, not
  // because the real claiming semantics are understood (see AGENTS.md's
  // "out of scope" note on this).
  bool claimed = false;
};

// --- Update queue / worker+display thread state -----------------------
// Moved here (from qsgepaper_globals.h) because listProcessedUpdates/
// accumList/listIncomingUpdates are real std::list<WorkItem>/std::list<Batch>
// members, which need WorkItem/Batch complete wherever this struct's
// implicit destructor gets instantiated - i.e. wherever
// update_queue_globals()'s function body (defining the static local
// instance below) is compiled. Every file that actually calls
// update_queue_globals() (swtcon.cpp, update.cpp, display.cpp)
// already includes this header; init.h/.cpp never needed this struct
// (see qsgepaper_globals.h's own note at the old location).
//
// This struct is native-owned storage (Phase 7) with no by-address ABI
// constraint of its own anymore, so - unlike WorkItem/RegionRows/BatchNode
// above - its field offsets don't need to match any real library address.
// The three list fields replacing a ListHead+int32-count pair are each
// exactly 12 bytes on this 32-bit target (matching libstdc++'s
// _List_node_header under _GLIBCXX_USE_CXX11_ABI=1: 2 pointers + a cached
// size_t), i.e. the exact same size as what they replace - not that it
// matters anymore, but worth noting this struct's shape didn't need to
// change shape to make room. The old version's `_reserved_0xNNNNN[...]`
// padding fields (there purely to preserve unreversed-library-.bss gaps at
// their real addresses) are gone too, for the same reason: nothing pins
// this struct to a real address anymore, so they'd just be dead bytes.
struct UpdateQueueGlobals {
  std::list<WorkItem> listProcessedUpdates;
  int32_t curFrame;
  int32_t targetFrame;
  pthread_mutex_t workerCondMutex;
  pthread_cond_t workerCond;
  uint8_t flashRequested; // byte flag, not int32
  pthread_mutex_t displayTimingMutex;
  Timespec64 lastPanTimestamp; // stamped alongside every nLastPannedFrame
                               // write, under displayTimingMutex
  sem_t displayThreadSem;
  int32_t timeVar;
  int32_t workerThreadShutdown;
  std::vector<ModeEntry*> waveform;
  int32_t shutdownRequested;
  std::list<Batch> listIncomingUpdates;
  pthread_mutex_t updateQueueMutex;
  pthread_t displayThread;
  pthread_t workerThread;
  void* dataBuffer;
  void* backBuffer;
  std::list<WorkItem> accumList;
  int16_t accumFlag;

  // Whether swtcon_init actually pthread_create'd the threads
  // (InitParams::startThreads) - swtcon_shutdown must only join those.
  bool threadsStarted = false;
};

inline UpdateQueueGlobals*
update_queue_globals() {
  static UpdateQueueGlobals g{};
  return &g;
}

// Shared internal primitives, also used by display.cpp (worker
// thread's flash sequence: temperature/LUT selection; display thread: list
// bookkeeping + work-item cloning/teardown, since display_thread_func
// manipulates the exact same lists swtcon_update does) - see
// update.cpp for definitions.
float get_current_temperature();
void select_waveform_lut(float temp, std::shared_ptr<LUTEntry>* out,
                                std::vector<ModeEntry*>* waveform, unsigned mode);
bool update_lut_is_valid(const std::shared_ptr<LUTEntry>& lut);

// Inserts `node` immediately before `pos` in a circular intrusive list (see
// update.cpp's definition for the full comment) - works for any
// ListHead-shaped sentinel/node (WorkItemNode, BatchNode). Kept for the
// probe tools (see BatchNode's comment); production code uses real
// std::list<T> operations instead.
void list_insert_before(void* pos, void* node);

// Unhooks `node` from whatever circular intrusive list it's currently in.
// Kept for the probe tools, same rationale as list_insert_before.
void list_unhook(void* node);

// Deep-copies a work item's shared_ptrs/list/scalars, preserving the
// source's rect and sequence id (see CloneWorkItemFieldsInto for the
// shared cloning body, also used by the rect-splitting piece-builder).
WorkItem* update_item_copy(WorkItem* dest, const WorkItem* src);

// Native reimplementation of dispatch_update_regions (0x4fff8) and its
// per-pixel kernel render_update_kernel (0x4e7b8) - see update.cpp
// for the full comment and swtcon_architecture.md §5.1/§5.2 for the
// confirmed formulas/addressing. Allocates item's RegionRows blob and fills
// it from dataBuffer/backBuffer (the full-screen working buffers).
void dispatch_update_regions(WorkItem* item, void* dataBuffer, void* backBuffer);

// Individual pieces of dispatch_update_regions/swtcon_update, non-static so
// each can be unit-tested directly - see update.cpp for the full comments.
Rect clamp_update_rect(const XYRect& in);
int render_kernel_case(int pixel_mode, int mode);
uint8_t render_kernel_formula(int case_, uint16_t src, bool back_active, uint8_t gamma);
void render_update_kernel(WorkItem* item, const uint16_t* dataBuffer, const uint8_t* backBuffer);
WorkItem* piece_builder(WorkItem* dest, const WorkItem* src, const Rect& piece_rect);
void subtract_update_region(std::list<WorkItem>& list, const WorkItem* new_item);
