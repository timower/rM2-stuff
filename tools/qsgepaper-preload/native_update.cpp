#include "native_update.h"
#include "native_init.h"
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <vector>

// Set to 1 to use the native re-implementations of update/lock/unlock/wait;
// 0 falls back to the library exports (for A/B comparison).
#define NATIVE_UPDATE 1

// Library exports resolved once at dlopen time (see swtcon.cpp's load_lib),
// used only by the NATIVE_UPDATE==0 fallback below.
extern void (*qsgepaper_lock)();
extern void (*qsgepaper_update)(update_data*);
extern void (*qsgepaper_unlock_post)();
extern void (*qsgepaper_wait)();

// Remaining library leaf routines swtcon_update still calls into by
// address (see AGENTS.md Phase 4b for the reversing status of each).
constexpr uintptr_t kMakeEmptyLutAddr = 0x408a8;
// The shared_ptr control block's vtable pointer for a RegionRows allocation,
// exactly matching dispatch_update_regions's own `*puVar1 = &PTR_LAB_000651ec`
// (see native_dispatch_update_regions below) - kept still-library (a generic
// libstdc++ dispose/destroy pair for an array-new'd byte buffer) so the
// existing release_sp/shared_ptr machinery keeps working on it unmodified.
constexpr uintptr_t kRegionRowsVtableAddr = 0x651ec;

// --- Native swtcon_update / lock / unlock_post / wait ---
//
// These mirror queue_update (0x3ccac), LockSwapMutex (0x3b690),
// unlock_and_post_swap / UnlockAndPostSwapMutex (0x3dd90) and WaitForUpdate
// (0x3b644). The native code owns the control flow, work-item field packing
// and the intrusive std::list / std::shared_ptr book-keeping (see WorkItem,
// WorkItemNode, BatchNode and RegionRows in native_update.h for the wire
// formats). Remaining library leaf routines (to be reversed in later
// steps):
//   0x1ec58 _M_hook                 - std::__detail::_List_node_base::_M_hook
//                                     (superseded here by list_insert_before,
//                                     a native reimplementation - kept as a
//                                     comment since other still-library code
//                                     paths, e.g. worker_thread_func, also
//                                     call the library's own copy)
//   0x408a8 (anonymous)             - allocate an empty LUTEntry + shared_ptr
//                                     control block (used for the +0x2c
//                                     placeholder in update_item_ctor, always
//                                     immediately replaced by select_waveform_lut)

// Release a libstdc++ _Sp_counted_base* exactly like the inlined shared_ptr
// destructor in queue_update: atomically drop the use-count, dispose on 0,
// then drop the weak-count and destroy on 0. vtable[2]=_M_dispose,
// vtable[3]=_M_destroy.
void
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

// Inserts `node` immediately before `pos` in a circular intrusive list.
// Works for any type whose first two members are next/prev pointers - i.e.
// every list-head sentinel (ListHead) and every node type in this file
// (WorkItemNode, BatchNode, IntListNode) - so `pos` may be a real node or
// the sentinel itself (inserting-before-the-sentinel == append-at-tail).
// Non-static: also used by native_display.cpp (see native_update.h).
void
list_insert_before(void* pos, void* node) {
  void** p = (void**)pos;
  void** n = (void**)node;
  void* prev = p[1];
  n[0] = pos;
  n[1] = prev;
  ((void**)prev)[0] = node;
  p[1] = node;
}

// Unhooks `node` from whatever circular intrusive list it's currently in.
// Non-static: also used by native_display.cpp (see native_update.h).
void
list_unhook(void* node) {
  void** n = (void**)node;
  void* next = n[0];
  void* prev = n[1];
  ((void**)prev)[0] = next;
  ((void**)next)[1] = prev;
}

// Frees a single work-item list node: frees the embedded std::list<int> at
// item.intList, releases the three shared_ptrs (regionRows, lut, sp3 - the
// same trio release_sp handles in swtcon_update's inline work-item
// destructor), then frees the node itself. Shared by
// native_free_update_region_list (whole-list teardown) and
// native_subtract_update_region (single-node removal when clipping).
// Non-static: also used by native_display.cpp's GC of g_pListProcessedUpdates
// (see native_update.h) - display_thread_func's node-teardown decompiles
// identically to this function (intList free, then sp3/lut/regionRows
// release, then operator delete, in that order).
void
native_destroy_item_node(WorkItemNode* node) {
  WorkItem& item = node->item;
  for (auto* n = (IntListNode*)item.intList.next; (void*)n != &item.intList;) {
    auto* next = n->next;
    ::operator delete(n);
    n = next;
  }
  release_sp(item.sp3.ctrl);
  release_sp(item.lut.ctrl);
  release_sp(item.regionRows.ctrl);
  ::operator delete(node);
}

// Native reimplementation of free_update_region_list (0x3e540): destroys and
// frees every work-item node in a circular intrusive list (used for both the
// pending-accumulation list and a claimed batch's region list).
// Non-static: also used by native_display.cpp (see native_update.h).
void
native_free_update_region_list(ListHead* list_head) {
  auto* node = (WorkItemNode*)list_head->next;
  if ((void*)node == (void*)list_head)
    return;
  do {
    auto* next = node->next;
    native_destroy_item_node(node);
    node = next;
  } while ((void*)node != (void*)list_head);
}

// Deep-copies everything a work item needs cloned (retained shared_ptrs,
// cloned embedded int-list, and every other scalar field verbatim) but does
// NOT touch the rect or sequence id. Shared by native_update_item_copy
// (which preserves the source's rect/seq id) and native_piece_builder
// (which overwrites both with the split-off piece's own).
static void
CloneWorkItemFieldsInto(WorkItem* dest, const WorkItem* src) {
  // regionRows shared_ptr, gap, rect, seq id, unknowns, LUT-width shorts.
  memcpy(dest, src, offsetof(WorkItem, lut));
  retain_sp(dest->regionRows.ctrl);

  dest->lut = src->lut; // shared_ptr<LUTEntry>
  retain_sp(dest->lut.ctrl);

  // mode, pad, temperature.
  memcpy(&dest->mode, &src->mode, offsetof(WorkItem, sp3) - offsetof(WorkItem, mode));

  dest->sp3 = src->sp3;
  retain_sp(dest->sp3.ctrl);

  memcpy(dest->_unknown0x44, src->_unknown0x44, sizeof(dest->_unknown0x44));

  dest->intList = { &dest->intList, &dest->intList };
  int count = 0;
  for (auto* n = (IntListNode*)src->intList.next; (const void*)n != &src->intList; n = n->next) {
    auto* node = (IntListNode*)::operator new(sizeof(IntListNode));
    node->value = n->value;
    list_insert_before(&dest->intList, node);
    count++;
  }
  dest->intListCount = count;

  // sync, fullRefresh, pad, pixelMode.
  memcpy(&dest->sync, &src->sync, sizeof(WorkItem) - offsetof(WorkItem, sync));
}

// Native reimplementation of update_item_copy (0x3e850).
// Non-static: also used by native_display.cpp (see native_update.h).
WorkItem*
native_update_item_copy(WorkItem* dest, const WorkItem* src) {
  CloneWorkItemFieldsInto(dest, src);
  return dest;
}

// Native reimplementation of update_item_ctor (0x3ffd0): zero-initialises the
// work item to a degenerate/empty rect {y0=0,x0=0,y1=-1,x1=-1}, a 25C default
// temperature, pixel_mode=5, and an empty self-referencing intrusive list
// head. Stamps the same global sequence counter the library itself uses
// (kSeqCounterAddr) so IDs stay unique. The placeholder LUT shared_ptr
// allocation is left to the library's own tiny allocator (kMakeEmptyLutAddr)
// - it's always released and replaced by select_waveform_lut before anything
// reads it, so there's no benefit to reversing it further.
static WorkItem*
native_update_item_ctor(WorkItem* item) {
  memset(item, 0, sizeof(WorkItem));
  item->rectY1 = -1;
  item->rectX1 = -1;
  item->temperature = 25.0f;
  item->intList = { &item->intList, &item->intList };
  item->pixelMode = 5;

  // Real signature is (void* out_sp, int size_kb, int mode_width, int
  // bit_depth, float temperature) - `temperature` is passed in a VFP
  // register (s0) per AAPCS-VFP, not as a 5th integer arg. size_kb=0 skips
  // the data-buffer allocation, so the other fields are irrelevant here:
  // this placeholder is always released and replaced by select_waveform_lut
  // before anything reads it.
  auto fn_make_empty_lut = resolve_ptr<void* (*)(void*, int, int, int, float)>(kMakeEmptyLutAddr);
  fn_make_empty_lut(&item->lut, 0, 0, 0, 0.0f);

  int* seq_counter = resolve_ptr<int*>(kSeqCounterAddr);
  item->seqId = ++(*seq_counter);
  return item;
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
static Rect
native_clamp_update_rect(const XYRect& in) {
  constexpr int32_t kMaxY = 0x74f; // SCREEN_HEIGHT - 1 = 1871
  constexpr int32_t kMaxX = 0x57b; // SCREEN_WIDTH - 1 = 1403

  auto non_neg = [](int v) { return v < 0 ? 0 : v; };
  auto clamp_hi = [](int v, int hi) { return v > hi ? hi : v; };

  Rect out;
  out.y0 = non_neg(kMaxY - in.y1) & ~7;
  out.x0 = non_neg(kMaxX - in.x1);
  out.y1 = clamp_hi(kMaxY - in.y0, kMaxY) | 7;
  out.x1 = clamp_hi(kMaxX - in.x0, kMaxX);
  return out;
}

// Native reimplementation of get_current_temperature (0x468a4): mutex-reads
// the library's own cached temperature global. The value is still produced
// by the library's background poll thread (FUN_0004681c: reads a hwmon
// sysfs path via fopen/strtol and subtracts a 2.0C calibration offset) -
// Phase 5 hasn't reimplemented that thread natively yet, so we read its
// output directly instead of re-polling hwmon ourselves.
float
native_get_current_temperature() {
  pthread_mutex_t* mutex = resolve_ptr<pthread_mutex_t*>(kTemperatureMutexAddr);
  float* cached_temp = resolve_ptr<float*>(kCachedTemperatureAddr);
  pthread_mutex_lock(mutex);
  float temp = *cached_temp;
  pthread_mutex_unlock(mutex);
  return temp;
}

// Native reimplementation of FUN_000400a8, the piece-builder used internally
// by subtract_update_region: clones `src` (via CloneWorkItemFieldsInto) but
// overwrites the rect with `piece_rect`, stamps a fresh sequence id, and
// re-bases the cached data-pointer field (`gap`) to the piece's new origin.
// `gap` is a pointer into the RegionRows buffer dispatch_update_regions
// (0x4fff8) allocates and hangs off `regionRows`; re-basing by
// stride*(piece.x0-src.x0) + (piece.y0-src.y0) keeps it pointing at the
// pixel data for the piece's own top-left corner (see RegionRows).
static WorkItem*
native_piece_builder(WorkItem* dest, const WorkItem* src, const Rect& piece_rect) {
  CloneWorkItemFieldsInto(dest, src);

  int32_t src_y0 = src->rectY0;
  int32_t src_x0 = src->rectX0;

  int* seq_counter = resolve_ptr<int*>(kSeqCounterAddr);
  dest->rectY0 = piece_rect.y0;
  dest->rectX0 = piece_rect.x0;
  dest->rectY1 = piece_rect.y1;
  dest->rectX1 = piece_rect.x1;
  dest->seqId = ++(*seq_counter);

  if (dest->regionRows.ptr) {
    auto* region_rows = (const RegionRows*)dest->regionRows.ptr;
    dest->gap += region_rows->stride * (piece_rect.x0 - src_x0) + (piece_rect.y0 - src_y0);
  }

  return dest;
}

// --- Native render_update_kernel / dispatch_update_regions (Phase 6) ---
//
// Both formulas and addressing are empirically confirmed against the real
// library function - see swtcon_architecture.md §5.1/§5.2 and
// tools/qsgepaper-preload/render_kernel_verify.cpp /
// render_kernel_addr_map.cpp for the verification methodology. The upshot:
// every output byte is a pure function of exactly one dataBuffer/backBuffer
// pixel and one gamma-table cell, read through a 180-degree rotation of the
// whole framebuffer relative to the update rect's own coordinate space (the
// same reflection native_clamp_update_rect already documents on the rect
// itself). dispatch_update_regions's two-way thread-pool chunking (for rects
// wider than 98 columns) only ever splits this same computation into two
// disjoint column ranges - it changes nothing about which output byte reads
// which input byte, so this single-pass native port doesn't replicate it.
constexpr int kScreenWidth = 1404;
constexpr int kScreenHeight = 1872;

extern void* g_pGammaTableNative;

// Mirrors render_update_kernel's pixelMode==5 ("auto") indirection through
// g_anPixelModeDispatchTable - confirmed by reading the table's bytes
// directly out of the loaded library (Ghidra address 0x596b8).
static int
native_render_kernel_case(int pixel_mode, int mode) {
  if (pixel_mode == 5) {
    static const int kTable[7] = { 6, 9, 9, 9, 9, 6, 8 };
    unsigned idx = (unsigned)(mode - 1);
    if (idx > 6)
      return -1; // out-of-range mode falls through to the default formula
    return kTable[idx];
  }
  return pixel_mode;
}

// Per-pixel formulas, transcribed from the ARM disassembly at 0x4e7b8 (the
// bitfield extraction, gamma-table add, and div-by-125-then-scale sequences
// were read directly off the umull/lsr reciprocal-division idiom).
static uint8_t
native_render_kernel_formula(int case_, uint16_t src, bool back_active, uint8_t gamma) {
  unsigned lo5 = src & 0x1f;
  unsigned mid6 = (src >> 5) & 0x3f;
  unsigned hi5 = src >> 11;
  switch (case_) {
    case 6:
    case 8:
      return (uint8_t)(((lo5 + mid6 + hi5 + gamma) / 125) * 30);
    case 7:
      if (!back_active)
        return 0x20;
      return (uint8_t)(((lo5 + mid6 + hi5 + gamma) / 125) * 30);
    case 9:
      return (uint8_t)((((lo5 + mid6 + hi5) * 15 + gamma) / 125) << 1);
    case 0xd:
      return 0x1e;
    default:
      return (uint8_t)(((lo5 + mid6 + hi5) >> 3) << 1);
  }
}

// Native reimplementation of render_update_kernel (0x4e7b8). `dataBuffer`
// and `backBuffer` are the full-screen working buffers (queue->dataBuffer /
// queue->backBuffer); item->regionRows.ptr must already point at a RegionRows
// sized for item's own rect (native_dispatch_update_regions's job).
static void
native_render_update_kernel(WorkItem* item, const uint16_t* dataBuffer, const uint8_t* backBuffer) {
  auto* rr = (RegionRows*)item->regionRows.ptr;
  if (!rr->dataPtr)
    return;
  const uint8_t* gammaTable = (const uint8_t*)g_pGammaTableNative;
  int case_ = native_render_kernel_case(item->pixelMode, item->mode);

  for (int32_t y_screen = item->rectY0; y_screen <= item->rectY1; y_screen++) {
    int row = y_screen - item->rectY0;
    int src_y = (kScreenHeight - 1) - y_screen;
    for (int32_t x_screen = item->rectX0; x_screen <= item->rectX1; x_screen++) {
      int col = x_screen - item->rectX0;
      int src_x = (kScreenWidth - 1) - x_screen;
      int srcIdx = src_y * kScreenWidth + src_x;
      uint8_t gamma = gammaTable[(src_x & 0x7f) + (src_y & 0x7f) * 0x88];
      uint8_t out =
        native_render_kernel_formula(case_, dataBuffer[srcIdx], backBuffer[srcIdx] != 0, gamma);
      rr->dataPtr[(int64_t)col * rr->stride + row] = out;
    }
  }
}

// Native reimplementation of dispatch_update_regions (0x4fff8): allocates the
// item's RegionRows blob (releasing whatever it had before) and fills it via
// native_render_update_kernel. Byte layout of the allocation exactly matches
// the library's own (vtable ptr + refcounts + RegionRows, 0x28 bytes total,
// RegionRows starting at +0xc - see swtcon_architecture.md §5.1) so the
// existing generic release_sp keeps working on it unmodified.
void
native_dispatch_update_regions(WorkItem* item, void* dataBuffer, void* backBuffer) {
  struct RegionRowsBlock {
    void* vtable;
    int32_t useCount;
    int32_t weakCount;
    RegionRows rr;
  };
  static_assert(offsetof(RegionRowsBlock, rr) == 0xc, "RegionRowsBlock layout drift");
  static_assert(sizeof(RegionRowsBlock) == 0x28, "RegionRowsBlock layout drift");

  auto* block = (RegionRowsBlock*)::operator new(sizeof(RegionRowsBlock));
  block->vtable = resolve_ptr<void*>(kRegionRowsVtableAddr);
  block->useCount = 1;
  block->weakCount = 1;
  block->rr.y0 = item->rectY0;
  block->rr.x0 = item->rectX0;
  block->rr.y1 = item->rectY1;
  block->rr.x1 = item->rectX1;
  block->rr.dataPtr = nullptr;
  block->rr.stride = 0;
  block->rr.size = 0;

  if (item->rectY0 <= item->rectY1 && item->rectX0 <= item->rectX1) {
    int32_t stride = ((item->rectY1 - item->rectY0) + 0x10) & ~0xf; // round_up(height, 16)
    int32_t size = stride * (item->rectX1 - item->rectX0 + 1);
    block->rr.stride = stride;
    block->rr.size = size;
    if (size != 0)
      block->rr.dataPtr = (uint8_t*)::operator new[](size);
  }

  void* old_ctrl = item->regionRows.ctrl;
  item->regionRows.ptr = &block->rr;
  item->regionRows.ctrl = block;
  release_sp(old_ctrl);
  item->gap = (int32_t)(intptr_t)block->rr.dataPtr;

  if (item->rectY0 <= item->rectY1 && item->rectX0 <= item->rectX1)
    native_render_update_kernel(item, (const uint16_t*)dataBuffer, (const uint8_t*)backBuffer);
}

// Native reimplementation of subtract_update_region (0x3be10): clips the
// newly-queued item's rect out of every overlapping region in `list` (either
// the pending accumulation list, or an unclaimed batch's region list).
// `count` is that same list's own item counter (accumCount for the
// accumulation list, or a BatchNode's own `count` field for a batch's
// sub-list).
//
// Algorithm (verified against the disassembly - the decompiled pseudocode
// here is misleading, see AGENTS.md): per node, skip on no AABB overlap;
// otherwise compute the intersection ("cut") rect. If cut == the node's own
// rect, the node is removed outright. Otherwise up to four leftover
// axis-aligned strips are emitted (left, top, bottom, right - each only if
// non-empty), each cloned from the old item via native_piece_builder, and the
// old node is replaced by them.
static void
native_subtract_update_region(ListHead* list, int32_t* count, const WorkItem* new_item) {
  Rect n{ new_item->rectY0, new_item->rectX0, new_item->rectY1, new_item->rectX1 };
  if (n.y1 < n.y0 || n.x1 < n.x0)
    return; // degenerate new rect - nothing to subtract

  auto* node = (WorkItemNode*)list->next;
  while ((void*)node != (void*)list) {
    auto* next = node->next;
    WorkItem& old = node->item;
    Rect o{ old.rectY0, old.rectX0, old.rectY1, old.rectX1 };

    bool degenerate_old = o.y1 < o.y0 || o.x1 < o.x0;
    bool no_overlap = n.x1 < o.x0 || o.x1 < n.x0 || n.y1 < o.y0 || o.y1 < n.y0;

    if (!degenerate_old && !no_overlap) {
      Rect cut;
      cut.y0 = n.y0 > o.y0 ? n.y0 : o.y0;
      cut.x0 = n.x0 > o.x0 ? n.x0 : o.x0;
      cut.y1 = n.y1 < o.y1 ? n.y1 : o.y1;
      cut.x1 = n.x1 < o.x1 ? n.x1 : o.x1;

      Rect pieces[4];
      int piece_count = 0;
      alignas(16) WorkItem tmp;

      bool full_containment = cut.y0 == o.y0 && cut.x0 == o.x0 && cut.y1 == o.y1 && cut.x1 == o.x1;

      if (!full_containment) {
        native_update_item_copy(&tmp, &old); // preserve LUT/mode/temp/flags

        if (o.x0 < cut.x0) pieces[piece_count++] = { o.y0, o.x0, o.y1, cut.x0 - 1 };
        if (o.y0 < cut.y0) pieces[piece_count++] = { o.y0, cut.x0, cut.y0 - 1, cut.x1 };
        if (cut.y1 < o.y1) pieces[piece_count++] = { cut.y1 + 1, cut.x0, o.y1, cut.x1 };
        if (cut.x1 < o.x1) pieces[piece_count++] = { o.y0, cut.x1 + 1, o.y1, o.x1 };
      }

      // Unhook and free the old node.
      list_unhook(node);
      native_destroy_item_node(node);
      (*count)--;

      for (int i = 0; i < piece_count; i++) {
        auto* new_node = (WorkItemNode*)::operator new(sizeof(WorkItemNode));
        native_piece_builder(&new_node->item, &tmp, pieces[i]);
        // Insert at the tail of the run of pieces just inserted (i.e.
        // immediately before `next`, in the old node's place).
        list_insert_before(next, new_node);
        (*count)++;
      }

      if (piece_count > 0) {
        // Release the temp copy's own shared_ptrs/list now that every piece
        // has retained/cloned what it needs.
        for (auto* n2 = (IntListNode*)tmp.intList.next; (void*)n2 != &tmp.intList;) {
          auto* nx = n2->next;
          ::operator delete(n2);
          n2 = nx;
        }
        release_sp(tmp.sp3.ctrl);
        release_sp(tmp.lut.ctrl);
        release_sp(tmp.regionRows.ctrl);
      }
    }

    node = next;
  }
}

// Native reimplementation of build_update_batch (0x3ea98): clones `accum`'s
// list into a fresh batch node (sub-list + count + mode copied from
// `accum_flag`), deep-copying each item via native_update_item_copy, then
// hooks the batch node into `incoming` immediately before `pos` (which may
// be a real BatchNode or the `incoming` sentinel itself) and bumps
// `incoming_count`.
static BatchNode*
native_build_update_batch(ListHead* incoming, int32_t* incoming_count, void* pos,
                           ListHead* accum, int16_t accum_flag) {
  auto* batch = (BatchNode*)::operator new(sizeof(BatchNode));
  batch->subList = { &batch->subList, &batch->subList };

  int count = 0;
  auto* src = (WorkItemNode*)accum->next;
  while ((void*)src != (void*)accum) {
    auto* next = src->next;
    auto* node = (WorkItemNode*)::operator new(sizeof(WorkItemNode));
    native_update_item_copy(&node->item, &src->item);
    list_insert_before(&batch->subList, node);
    count++;
    src = next;
  }
  batch->count = count;
  batch->mode = accum_flag;

  list_insert_before(pos, batch);
  (*incoming_count)++;

  (void)incoming;
  return batch;
}

// Native reimplementation of select_waveform_lut (0x4535c): picks the LUT
// shared_ptr for `mode`'s waveform entry whose temperature bucket contains
// `temp`. Each ModeEntry's `luts` vector is sorted ascending by
// LUTEntry::temperature; this scans from index 1, keeping the highest index
// whose threshold `temp` still meets or exceeds, and stops at the first
// index whose threshold `temp` falls short of (so a single-entry vector
// trivially resolves to index 0 without a special case). Falls back to an
// empty placeholder LUT (via the library's own tiny allocator,
// kMakeEmptyLutAddr - same one update_item_ctor uses) if `mode` is out of
// range or its ModeEntry has no LUTs at all.
void
native_select_waveform_lut(float temp, SpRef* out, std::vector<ModeEntry*>* waveform, unsigned mode) {
  if (mode < waveform->size()) {
    ModeEntry* m = (*waveform)[mode];
    auto& luts = m->luts;
    if (!luts.empty()) {
      size_t selected = 0;
      for (size_t i = 1; i < luts.size(); i++) {
        if (temp < luts[i]->temperature)
          break;
        selected = i;
      }
      *out = reinterpret_cast<SpRef&>(luts[selected]); // shared_ptr<LUTEntry> == SpRef (see SpRef's comment)
      retain_sp(out->ctrl);
      return;
    }
  }

  auto fn_make_empty_lut = resolve_ptr<void* (*)(void*, int, int, int, float)>(kMakeEmptyLutAddr);
  fn_make_empty_lut(out, 0, 0, 0, 0.0f);
}

// Native reimplementation of update_lut_is_valid (0x409e4): sanity-checks
// the LUT select_waveform_lut just picked - non-null pixel data, and
// positive size_kb/bit_depth/mode_width.
bool
native_update_lut_is_valid(const SpRef& lut) {
  auto* entry = (const LUTEntry*)lut.ptr;
  if (!entry->data)
    return false;
  if (entry->size_kb > 0 && entry->bit_depth > 0)
    return entry->mode_width > 0;
  return false;
}

void
swtcon_lock() {
#if NATIVE_UPDATE
  // LockSwapMutex: take the update-queue mutex.
  pthread_mutex_lock(&update_queue_globals()->updateQueueMutex);
#else
  qsgepaper_lock();
#endif
}

void
swtcon_update(update_data* data) {
#if NATIVE_UPDATE
  auto* queue = update_queue_globals();

  alignas(16) WorkItem item;
  native_update_item_ctor(&item); // sets seq id, empties the shared_ptrs / list

  // The input rect {y,x,height,width} is byte-reversed per 64-bit lane
  // (vrev64.32) to {x,y,width,height} before clamping.
  XYRect rev{ data->x, data->y, data->width, data->height };
  Rect rect = native_clamp_update_rect(rev);
  item.rectY0 = rect.y0;
  item.rectX0 = rect.x0;
  item.rectY1 = rect.y1;
  item.rectX1 = rect.x1;

  int seq = item.seqId; // return value (unused by callers today)

  // Reject empty/degenerate rectangles (x1>=x0 && y0<=y1).
  bool ok = !(rect.y1 < rect.y0) && !(rect.x0 > rect.x1);

  if (ok) {
    int flags = data->flags;
    item.pixelMode = data->pixel_mode;
    item.mode = (int16_t)data->update_mode;
    item.sync = (uint8_t)(flags & 1);        // Sync
    item.fullRefresh = (uint8_t)((flags >> 1) & 1); // FullRefresh
    float temp = native_get_current_temperature();
    if (flags & 8) // FastDraw: caller supplies an explicit temperature
      temp = (float)data->zero;
    item.temperature = temp;

    native_dispatch_update_regions(&item, queue->dataBuffer, queue->backBuffer);

    // Select the LUT and move the returned shared_ptr into the item,
    // releasing whatever was there (empty after the ctor).
    SpRef selected{};
    native_select_waveform_lut(temp, &selected, (std::vector<ModeEntry*>*)(void*)queue->waveformStructRaw,
                                (unsigned)(int)item.mode);
    release_sp(item.lut.ctrl);
    item.lut = selected;

    if (native_update_lut_is_valid(item.lut)) {
      auto* lut = (const int32_t*)item.lut.ptr;
      item.lutWidthMinus1 = (int16_t)(lut[0] - 1); // packed LUT width - 1

      // Clip this rectangle out of the pending accumulation regions and out of
      // every not-yet-locked batch already queued for the worker.
      native_subtract_update_region(&queue->accumList, &queue->accumCount, &item);
      for (auto* b = (BatchNode*)queue->listIncomingUpdates.next;
           (void*)b != &queue->listIncomingUpdates; b = b->next) {
        if (!BatchNodeClaimed(b))
          native_subtract_update_region(&b->subList, &b->count, &item);
      }

      // Enqueue: 100-byte list node, deep-copy the item into node+8, hook it at
      // the tail of the accumulation list, bump the count.
      auto* node = (WorkItemNode*)::operator new(sizeof(WorkItemNode));
      native_update_item_copy(&node->item, &item);
      list_insert_before(&queue->accumList, node);
      queue->accumCount++;
    } else {
      seq = -1;
    }
  } else {
    seq = -1;
  }

  // Work-item destructor (inlined tail of queue_update): free the embedded
  // std::list<int>, then release the three shared_ptrs.
  for (auto* n = (IntListNode*)item.intList.next; (void*)n != &item.intList;) {
    auto* nx = n->next;
    ::operator delete(n);
    n = nx;
  }
  release_sp(item.sp3.ctrl);
  release_sp(item.lut.ctrl);
  release_sp(item.regionRows.ctrl);
  (void)seq;
#else
  qsgepaper_update(data);
#endif
}

void
swtcon_unlock_post() {
#if NATIVE_UPDATE
  auto* queue = update_queue_globals();

  // Find the insertion point: walk backwards from the head, skipping batches
  // already claimed by the worker, so this batch is queued after them but
  // before any pending ones.
  void* first = queue->listIncomingUpdates.next;
  void* pos = &queue->listIncomingUpdates;
  while (pos != first) {
    void* prev = ((void**)pos)[1];
    if (!BatchNodeClaimed((BatchNode*)prev))
      break;
    pos = prev;
  }

  // Clone the accumulated regions into a fresh batch node hooked before
  // `pos`, then free the originals and reset the accumulation list to empty.
  native_build_update_batch(&queue->listIncomingUpdates, &queue->incomingBatchCount, pos,
                             &queue->accumList, queue->accumFlag);
  native_free_update_region_list(&queue->accumList);
  queue->accumList = { &queue->accumList, &queue->accumList };
  queue->accumCount = 0;
  queue->accumFlag = 0;

  pthread_mutex_unlock(&queue->updateQueueMutex);
  sem_post(&queue->displayThreadSem); // wake the display thread
#else
  qsgepaper_unlock_post();
#endif
}

void
swtcon_wait() {
#if NATIVE_UPDATE
  // WaitForUpdate: spin until shutdown or the batch queue drains.
  auto* queue = update_queue_globals();
  while (queue->shutdownRequested == 0 &&
         queue->listIncomingUpdates.next != &queue->listIncomingUpdates) {
    usleep(100);
  }
#else
  qsgepaper_wait();
#endif
}
