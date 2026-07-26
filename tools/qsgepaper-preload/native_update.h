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
#include <vector>

#include "native_init.h" // ModeEntry
#include "qsgepaper_globals.h"
#include "swtcon.h"

// A shared_ptr as the library inlines it: the aliased raw pointer plus the
// control-block pointer (release_sp/retain_sp operate on `ctrl`).
struct SpRef {
  void* ptr;
  void* ctrl;
};
static_assert(sizeof(SpRef) == 8, "SpRef must be two pointers");

// A work-item rect in the library's native {y0,x0,y1,x1} field order (see
// WorkItem below).
struct Rect {
  int32_t y0, x0, y1, x1;
};

// The pre-clamp rect order swtcon_update builds from update_data's
// x/y/width/height (see native_clamp_update_rect's comment for why this
// isn't the same axis order as Rect).
struct XYRect {
  int32_t x0, y0, x1, y1;
};

// dispatch_update_regions's (0x4fff8) output, hung off a work item's
// `regionRows` shared_ptr - the raw pointer half points directly at this
// struct. Column-major: address(y,x) = dataPtr + stride*(x-x0) + (y-y0).
struct RegionRows {
  uint8_t* dataPtr;  // size = stride * (x1-x0+1)
  int32_t y0, x0, y1, x1;
  int32_t stride;  // round_up(y1-y0+1, 16)
  int32_t size;
};
static_assert(sizeof(RegionRows) == 0x1c, "RegionRows layout drift");

// A node of the work item's embedded std::list<int> at +0x48. Despite the
// "int" in std::list<int>, `value` (confirmed via disassembly of
// build_overlap_dependency_list, 0x3a838) actually holds a raw `WorkItem*`
// pointing at another in-flight item this one's rendering depends on - not
// a scalar id/count. Kept as int32_t here (same size on this 32-bit target)
// rather than WorkItem* to avoid a circular type dependency; cast at each
// use site instead.
struct IntListNode {
  IntListNode* next;
  IntListNode* prev;
  int32_t value; // actually a WorkItem* - see comment above
};
static_assert(sizeof(IntListNode) == 0xc, "IntListNode layout drift");

// The library's internal "update work item", 0x5c bytes. Mirrors
// update_item_ctor (0x3ffd0). Lives embedded at +8 inside a WorkItemNode.
struct WorkItem {
  SpRef regionRows;          // +0x00 shared_ptr<RegionRows> (dispatch_update_regions's output)
  int32_t gap;                // +0x08 cached RegionRows::dataPtr, rebased to this item's rect origin (see native_piece_builder)
  int32_t rectY0;               // +0x0c
  int32_t rectX0;                // +0x10
  int32_t rectY1;                 // +0x14
  int32_t rectX1;                  // +0x18
  int32_t seqId;                    // +0x1c
  uint8_t _unknown0x20[8];           // +0x20..+0x27 unreversed
  int16_t _zero0x28;                  // +0x28 always observed 0
  int16_t lutWidthMinus1;               // +0x2a
  SpRef lut;                             // +0x2c shared_ptr<LUTEntry> (selected waveform LUT)
  int16_t mode;                           // +0x34
  int16_t _pad0x36;
  float temperature;                       // +0x38
  SpRef sp3;                                // +0x3c shared_ptr, purpose unreversed
  uint8_t _unknown0x44[4];                   // +0x44 unreversed, adjacent to sp3
  ListHead intList;                           // +0x48 std::list<int> head (IntListNode)
  int32_t intListCount;                        // +0x50
  uint8_t sync;                                 // +0x54
  uint8_t fullRefresh;                           // +0x55
  uint8_t _pad0x56[2];
  int32_t pixelMode;                              // +0x58
};
#define WI_ASSERT(field, off) \
  static_assert(offsetof(WorkItem, field) == (off), #field " must land at " #off)
WI_ASSERT(gap, 0x08);
WI_ASSERT(rectY0, 0x0c);
WI_ASSERT(rectX0, 0x10);
WI_ASSERT(rectY1, 0x14);
WI_ASSERT(rectX1, 0x18);
WI_ASSERT(seqId, 0x1c);
WI_ASSERT(lutWidthMinus1, 0x2a);
WI_ASSERT(lut, 0x2c);
WI_ASSERT(mode, 0x34);
WI_ASSERT(temperature, 0x38);
WI_ASSERT(sp3, 0x3c);
WI_ASSERT(intList, 0x48);
WI_ASSERT(intListCount, 0x50);
WI_ASSERT(sync, 0x54);
WI_ASSERT(fullRefresh, 0x55);
WI_ASSERT(pixelMode, 0x58);
#undef WI_ASSERT
static_assert(sizeof(WorkItem) == 0x5c, "WorkItem layout drift");

// A work item's containing intrusive-list node - 100 bytes total, matching
// every `operator_new(100)` / node+8 pattern in the library.
struct WorkItemNode {
  WorkItemNode* next;
  WorkItemNode* prev;
  WorkItem item;
};
static_assert(sizeof(WorkItemNode) == 100, "WorkItemNode must match the library's 100-byte list node");

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
static_assert(offsetof(BatchNode, subList) == 0x08, "");
static_assert(offsetof(BatchNode, count) == 0x10, "");
static_assert(offsetof(BatchNode, mode) == 0x14, "");
static_assert(sizeof(BatchNode) == 0x18, "BatchNode layout drift");

// True if the worker thread has already claimed this batch (won't be
// touched again by swtcon_update/unlock_post's subtract/enqueue paths).
inline bool
BatchNodeClaimed(const BatchNode* b) {
  return *((const uint8_t*)b + 0x15) != 0;
}

// Shared internal primitives, also used by native_display.cpp's worker
// thread (flash sequence: temperature/LUT selection + shared_ptr release) -
// see native_update.cpp for definitions.
void release_sp(void* ctrl);
float native_get_current_temperature();
void native_select_waveform_lut(float temp, SpRef* out, std::vector<ModeEntry*>* waveform, unsigned mode);
bool native_update_lut_is_valid(const SpRef& lut);
