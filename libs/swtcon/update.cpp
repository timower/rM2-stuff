#include "update.h"
#include "init.h"
#include "swtcon_libimpl.h"
#include <cstring>
#include <iterator>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <utility>
#include <vector>

// Library exports resolved once at dlopen time (see swtcon.cpp's load_lib),
// used only by the swtcon_lib_impl_enabled() fallback below - only defined
// there (and meaningful) against the real armv7 binary, see
// SWTCON_32BIT_ABI_CHECK's own comment in qsgepaper_globals.h.
#if SWTCON_32BIT_ABI_CHECK
extern void (*qsgepaper_lock)();
extern void (*qsgepaper_update)(update_data*);
extern void (*qsgepaper_unlock_post)();
extern void (*qsgepaper_wait)();
#endif // SWTCON_32BIT_ABI_CHECK

// --- Native swtcon_update / lock / unlock_post / wait ---
//
// These mirror queue_update (0x3ccac), LockSwapMutex (0x3b690),
// unlock_and_post_swap / UnlockAndPostSwapMutex (0x3dd90) and WaitForUpdate
// (0x3b644). The native code owns the control flow, work-item field packing
// and the intrusive std::list / std::shared_ptr book-keeping (see WorkItem,
// WorkItemNode, BatchNode and RegionRows in update.h for the wire
// formats). Remaining library leaf routines (to be reversed in later
// steps):
//   0x1ec58 _M_hook                 - std::__detail::_List_node_base::_M_hook
//                                     (superseded here by list_insert_before,
//                                     a native reimplementation - kept as a
//                                     comment since other still-library code
//                                     paths, e.g. worker_thread_func, also
//                                     call the library's own copy)

// Inserts `node` immediately before `pos` in a circular intrusive list.
// Works for any type whose first two members are next/prev pointers - i.e.
// every list-head sentinel (ListHead) and every node type in this file
// (WorkItemNode, BatchNode) - so `pos` may be a real node or the sentinel
// itself (inserting-before-the-sentinel == append-at-tail). Kept for the
// probe tools only - production code uses real std::list<T> now (Step 4).
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
// Kept for the probe tools only, same rationale as list_insert_before.
void
list_unhook(void* node) {
  void** n = (void**)node;
  void* next = n[0];
  void* prev = n[1];
  ((void**)prev)[0] = next;
  ((void**)next)[1] = prev;
}

// Deep-copies everything a work item needs cloned (retained shared_ptrs,
// copied deps vector, and every other scalar field verbatim) but does NOT
// touch the rect or sequence id. Shared by update_item_copy (which
// preserves the source's rect/seq id) and piece_builder (which
// overwrites both with the split-off piece's own).
static void
CloneWorkItemFieldsInto(WorkItem* dest, const WorkItem* src) {
  // shared_ptr copy-assignment does its own retain (and releases whatever
  // dest previously held) - no manual refcounting needed anymore.
  dest->regionRows = src->regionRows;

  // pixelDataPtr, rect, seq id, frameCursor/frameAnchor, phase, LUT-width
  // shorts - every scalar field between regionRows and lut.
  memcpy(&dest->pixelDataPtr,
         &src->pixelDataPtr,
         offsetof(WorkItem, lut) - offsetof(WorkItem, pixelDataPtr));

  dest->lut = src->lut;

  // mode, pad, temperature.
  memcpy(&dest->mode,
         &src->mode,
         offsetof(WorkItem, pixelTransitions) - offsetof(WorkItem, mode));

  dest->pixelTransitions = src->pixelTransitions;

  dest->transitionDataPtr =
    src->transitionDataPtr; // +0x44 cached pixelTransitions buffer ptr (see
                            // WorkItem def)

  dest->deps = src->deps;

  // sync, fastDraw, pad, pixelMode.
  memcpy(&dest->sync, &src->sync, sizeof(WorkItem) - offsetof(WorkItem, sync));
}

// Native reimplementation of update_item_copy (0x3e850).
// Non-static: also used by display.cpp (see update.h).
WorkItem*
update_item_copy(WorkItem* dest, const WorkItem* src) {
  CloneWorkItemFieldsInto(dest, src);
  return dest;
}

// Native reimplementation of the anonymous LUTEntry+shared_ptr allocator at
// 0x408a8 (confirmed signature via disassembly: r0=out_sp, r1=size_kb,
// r2=mode_width, r3=bit_depth, s0=temperature per AAPCS-VFP). Both real call
// sites (update_item_ctor's placeholder, select_waveform_lut's
// out-of-range-mode fallback) always pass all-zero fields - immediately
// released and replaced before anything reads them - so this only needs to
// build a valid, empty, ref-counted LUTEntry. LUTEntry's own destructor
// (init.h/.cpp) already frees `.data`, so a plain make_shared is
// enough - no custom deleter needed here (unlike RegionRows's dataPtr).
static void
make_empty_lut(std::shared_ptr<LUTEntry>* out) {
  *out = std::make_shared<LUTEntry>();
}

// Native reimplementation of update_item_ctor (0x3ffd0): zero-initialises the
// work item to a degenerate/empty rect {y0=0,x0=0,y1=-1,x1=-1}, a 25C default
// temperature, pixel_mode=5, and an empty deps list. Stamps the same global
// sequence counter the library itself uses (kSeqCounterAddr) so IDs stay
// unique.
static WorkItem*
update_item_ctor(WorkItem* item) {
  // Zero every scalar/POD field. The three shared_ptr fields (regionRows,
  // lut, pixelTransitions) and `deps` are already valid,
  // default-constructed-empty objects as soon as `*item` exists (the
  // caller's `WorkItem item;` local, or a WorkItemNode's own constructor) -
  // memset'ing over a live shared_ptr/vector would bypass its assignment
  // operator, so those spans are skipped here rather than blitted over
  // directly.
  memset(&item->pixelDataPtr,
         0,
         offsetof(WorkItem, lut) - offsetof(WorkItem, pixelDataPtr));
  memset(&item->mode,
         0,
         offsetof(WorkItem, pixelTransitions) - offsetof(WorkItem, mode));
  memset(&item->transitionDataPtr,
         0,
         offsetof(WorkItem, deps) - offsetof(WorkItem, transitionDataPtr));
  memset(&item->sync, 0, sizeof(WorkItem) - offsetof(WorkItem, sync));

  item->rectY1 = -1;
  item->rectX1 = -1;
  item->temperature = 25.0f;
  item->deps.clear();
  item->pixelMode = 5;

  make_empty_lut(&item->lut);

  int* seq_counter = seq_counter_ptr();
  item->seqId = ++(*seq_counter);
  return item;
}

// Native reimplementation of clamp_update_rect (0x4fc40). Input is
// {x0,y0,x1,y1} (queue_update passes update_data's x0/y0/x1/y1 reordered
// this way - update_data's x1/y1 are opposite-corner coordinates, not sizes,
// hence the rename from the library's original "width"/"height" naming:
// main.cpp's full-screen requests set them to SCREEN_WIDTH/SCREEN_HEIGHT,
// matching the 1403/1871 constants below exactly). The
// transform is an independent per-axis point reflection through
// (SCREEN_HEIGHT-1, SCREEN_WIDTH-1) - i.e. it flips the rect into the
// panel's 180-rotated hardware frame - with the y-axis rounded to 8-row
// blocks (down for y0, up for y1) to match the 8-row-aligned render/dispatch
// kernels. Output order is {y0,x0,y1,x1}.
//
// Non-static (declared in update.h) for direct unit testing - not a hot
// per-pixel path, so this costs nothing.
Rect
clamp_update_rect(const XYRect& in) {
  constexpr int32_t kMaxY = kScreenHeight - 1;
  constexpr int32_t kMaxX = kScreenWidth - 1;

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
get_current_temperature() {
  pthread_mutex_t* mutex = temperature_mutex();
  float* cached_temp = cached_temperature_ptr();
  pthread_mutex_lock(mutex);
  float temp = *cached_temp;
  pthread_mutex_unlock(mutex);
  return temp;
}

// Native reimplementation of FUN_000400a8, the piece-builder used internally
// by subtract_update_region: clones `src` (via CloneWorkItemFieldsInto) but
// overwrites the rect with `piece_rect`, stamps a fresh sequence id, and
// re-bases the cached data-pointer field (`pixelDataPtr`) to the piece's new
// origin. `pixelDataPtr` is a pointer into the RegionRows buffer
// dispatch_update_regions (0x4fff8) allocates and hangs off `regionRows`;
// re-basing by stride*(piece.x0-src.x0) + (piece.y0-src.y0) keeps it
// pointing at the pixel data for the piece's own top-left corner (see
// RegionRows).
//
// Non-static (declared in update.h) for direct unit testing - not a hot path.
WorkItem*
piece_builder(WorkItem* dest, const WorkItem* src, const Rect& piece_rect) {
  CloneWorkItemFieldsInto(dest, src);

  int32_t src_y0 = src->rectY0;
  int32_t src_x0 = src->rectX0;

  int* seq_counter = seq_counter_ptr();
  dest->rectY0 = piece_rect.y0;
  dest->rectX0 = piece_rect.x0;
  dest->rectY1 = piece_rect.y1;
  dest->rectX1 = piece_rect.x1;
  dest->seqId = ++(*seq_counter);

  if (dest->regionRows) {
    dest->pixelDataPtr += dest->regionRows->stride * (piece_rect.x0 - src_x0) +
                          (piece_rect.y0 - src_y0);
  }

  return dest;
}

// --- Native render_update_kernel / dispatch_update_regions (Phase 6) ---
//
// Both formulas and addressing are empirically confirmed against the real
// library function - see swtcon_architecture.md §5.1/§5.2 for the
// verification methodology. The upshot:
// every output byte is a pure function of exactly one dataBuffer/backBuffer
// pixel and one gamma-table cell, read through a 180-degree rotation of the
// whole framebuffer relative to the update rect's own coordinate space (the
// same reflection clamp_update_rect already documents on the rect
// itself). dispatch_update_regions's two-way thread-pool chunking (for rects
// wider than 98 columns) only ever splits this same computation into two
// disjoint column ranges - it changes nothing about which output byte reads
// which input byte, so this single-pass native port doesn't replicate it.

// Mirrors render_update_kernel's pixelMode==5 ("auto") indirection through
// g_anPixelModeDispatchTable - confirmed by reading the table's bytes
// directly out of the loaded library (Ghidra address 0x596b8).
//
// Non-static (declared in update.h) for direct unit testing - called once
// per work item, outside the per-pixel loop below, not a hot path.
int
render_kernel_case(int pixel_mode, int mode) {
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
//
// Non-static (declared in update.h) for direct unit testing - but this is
// the true hot path (per-pixel, from render_update_kernel's loop below).
//
// `always_inline` keeps it inlined there under -Os; `used` is required
// alongside it or GCC drops the out-of-line copy a test TU needs to link
// against (both confirmed via objdump on the real armv7 release build).
[[gnu::always_inline, gnu::used]] inline uint8_t
render_kernel_formula(int case_,
                      uint16_t src,
                      bool back_active,
                      uint8_t gamma) {
  unsigned lo5 = src & 0x1f;
  unsigned mid6 = (src >> 5) & 0x3f;
  unsigned hi5 = src >> 11;
  switch (case_) {
    case 6:
    case 8:
      return (uint8_t)(((lo5 + mid6 + hi5 + gamma) / 125) * 30);
    case 7:
      if (!back_active)
        return (uint8_t)kGatedPixelSentinel;
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
// queue->backBuffer); item->regionRows must already point at a RegionRows
// sized for item's own rect (dispatch_update_regions's job).
//
// Non-static (declared in update.h) for direct unit testing - a test needs
// init_statebuffer() to have populated statebuffer_globals()->pGammaTable
// first.
void
render_update_kernel(WorkItem* item,
                     const uint16_t* dataBuffer,
                     const uint8_t* backBuffer) {
  auto* rr = item->regionRows.get();
  if (!rr->dataPtr)
    return;
  const uint8_t* gammaTable =
    (const uint8_t*)statebuffer_globals()->pGammaTable;
  int case_ = render_kernel_case(item->pixelMode, item->mode);

  for (int32_t y_screen = item->rectY0; y_screen <= item->rectY1; y_screen++) {
    int row = y_screen - item->rectY0;
    int src_y = (kScreenHeight - 1) - y_screen;
    for (int32_t x_screen = item->rectX0; x_screen <= item->rectX1;
         x_screen++) {
      int col = x_screen - item->rectX0;
      int src_x = (kScreenWidth - 1) - x_screen;
      int srcIdx = src_y * kScreenWidth + src_x;
      uint8_t gamma = gammaTable[(src_x & 0x7f) + (src_y & 0x7f) * 0x88];
      uint8_t out = render_kernel_formula(
        case_, dataBuffer[srcIdx], backBuffer[srcIdx] != 0, gamma);
      rr->dataPtr[(int64_t)col * rr->stride + row] = out;
    }
  }
}

// Native reimplementation of dispatch_update_regions (0x4fff8): allocates the
// item's RegionRows blob (releasing whatever it had before, via ordinary
// shared_ptr assignment) and fills it via render_update_kernel.
void
dispatch_update_regions(WorkItem* item, void* dataBuffer, void* backBuffer) {
  auto* rr = new RegionRows{};
  rr->y0 = item->rectY0;
  rr->x0 = item->rectX0;
  rr->y1 = item->rectY1;
  rr->x1 = item->rectX1;
  rr->dataPtr = nullptr;
  rr->stride = 0;
  rr->size = 0;

  if (item->rectY0 <= item->rectY1 && item->rectX0 <= item->rectX1) {
    int32_t stride = ((item->rectY1 - item->rectY0) + kRegionRowsStrideAlign) &
                     ~(kRegionRowsStrideAlign - 1); // round_up(height, 16)
    int32_t size = stride * (item->rectX1 - item->rectX0 + 1);
    rr->stride = stride;
    rr->size = size;
    if (size != 0)
      rr->dataPtr = new uint8_t[size];
  }

  // RegionRows::dataPtr is a plain non-owning pointer (see its comment in
  // update.h) - this is the one site that actually owns the buffer it
  // just allocated, so free it explicitly via a custom deleter rather than
  // giving RegionRows itself an owning destructor.
  item->regionRows = std::shared_ptr<RegionRows>(rr, [](RegionRows* p) {
    delete[] p->dataPtr;
    delete p;
  });
  item->pixelDataPtr = (uintptr_t)rr->dataPtr;

  if (item->rectY0 <= item->rectY1 && item->rectX0 <= item->rectX1)
    render_update_kernel(
      item, (const uint16_t*)dataBuffer, (const uint8_t*)backBuffer);
}

// Native reimplementation of subtract_update_region (0x3be10): clips the
// newly-queued item's rect out of every overlapping region in `list` (either
// the pending accumulation list, or an unclaimed batch's sub-list).
//
// Algorithm (verified against the disassembly - the decompiled pseudocode
// here is misleading, see AGENTS.md): per item, skip on no AABB overlap;
// otherwise compute the intersection ("cut") rect. If cut == the item's own
// rect, the item is removed outright. Otherwise up to four leftover
// axis-aligned strips are emitted (left, top, bottom, right - each only if
// non-empty), each cloned from the old item via piece_builder, and the
// old item is replaced by them (in the same position, preserving order).
//
// Non-static (declared in update.h) for direct unit testing - not a hot path.
void
subtract_update_region(std::list<WorkItem>& list, const WorkItem* new_item) {
  Rect n{
    new_item->rectY0, new_item->rectX0, new_item->rectY1, new_item->rectX1
  };
  if (n.y1 < n.y0 || n.x1 < n.x0)
    return; // degenerate new rect - nothing to subtract

  for (auto it = list.begin(); it != list.end();) {
    WorkItem& old = *it;
    Rect o{ old.rectY0, old.rectX0, old.rectY1, old.rectX1 };

    bool degenerate_old = o.y1 < o.y0 || o.x1 < o.x0;
    bool no_overlap = n.x1 < o.x0 || o.x1 < n.x0 || n.y1 < o.y0 || o.y1 < n.y0;

    if (degenerate_old || no_overlap) {
      ++it;
      continue;
    }

    Rect cut;
    cut.y0 = n.y0 > o.y0 ? n.y0 : o.y0;
    cut.x0 = n.x0 > o.x0 ? n.x0 : o.x0;
    cut.y1 = n.y1 < o.y1 ? n.y1 : o.y1;
    cut.x1 = n.x1 < o.x1 ? n.x1 : o.x1;

    Rect pieces[4];
    int piece_count = 0;
    WorkItem tmp;

    bool full_containment =
      cut.y0 == o.y0 && cut.x0 == o.x0 && cut.y1 == o.y1 && cut.x1 == o.x1;

    if (!full_containment) {
      update_item_copy(&tmp, &old); // preserve LUT/mode/temp/flags

      if (o.x0 < cut.x0)
        pieces[piece_count++] = { o.y0, o.x0, o.y1, cut.x0 - 1 };
      if (o.y0 < cut.y0)
        pieces[piece_count++] = { o.y0, cut.x0, cut.y0 - 1, cut.x1 };
      if (cut.y1 < o.y1)
        pieces[piece_count++] = { cut.y1 + 1, cut.x0, o.y1, cut.x1 };
      if (cut.x1 < o.x1)
        pieces[piece_count++] = { o.y0, cut.x1 + 1, o.y1, o.x1 };
    }

    // Erase the old element; `insert_pos` is what followed it (the
    // original `next`), matching the original's "insert every piece
    // immediately before `next`" - std::list::insert never invalidates
    // insert_pos, so all pieces land in order right before it.
    auto insert_pos = list.erase(it);

    for (int i = 0; i < piece_count; i++) {
      WorkItem piece;
      piece_builder(&piece, &tmp, pieces[i]);
      list.insert(insert_pos, std::move(piece));
    }
    // tmp's shared_ptr fields and deps vector release/free themselves
    // automatically when tmp goes out of scope below - no manual cleanup
    // needed anymore, regardless of piece_count.

    it = insert_pos;
  }
}

// Native reimplementation of build_update_batch (0x3ea98): moves `accum`'s
// items into a fresh batch's sub-list (mode copied from `accum_flag`), then
// hooks the batch into `incoming` immediately before `pos`. `accum`'s own
// items are left moved-from (empty) - the caller (swtcon_unlock_post) clears
// `accum` right after, matching the original's separate
// free_update_region_list call except without the redundant
// retain-then-release round trip a copy would need (accum's originals are
// getting destroyed either way, so moving is strictly cheaper and
// behaviorally identical - see subtract_update_region's similar
// move-based erase for the same reasoning applied elsewhere in this file).
static void
build_update_batch(std::list<Batch>& incoming,
                   std::list<Batch>::iterator pos,
                   std::list<WorkItem>& accum,
                   int16_t accum_flag) {
  Batch batch;
  for (auto& item : accum)
    batch.subList.push_back(std::move(item));
  batch.mode = accum_flag;

  incoming.insert(pos, std::move(batch));
}

// Native reimplementation of select_waveform_lut (0x4535c): picks the LUT
// shared_ptr for `mode`'s waveform entry whose temperature bucket contains
// `temp`. Each ModeEntry's `luts` vector is sorted ascending by
// LUTEntry::temperature; this scans from index 1, keeping the highest index
// whose threshold `temp` still meets or exceeds, and stops at the first
// index whose threshold `temp` falls short of (so a single-entry vector
// trivially resolves to index 0 without a special case). Falls back to an
// empty placeholder LUT (make_empty_lut, same one
// update_item_ctor uses) if `mode` is out of range or its ModeEntry
// has no LUTs at all.
void
select_waveform_lut(float temp,
                    std::shared_ptr<LUTEntry>* out,
                    std::vector<ModeEntry*>* waveform,
                    unsigned mode) {
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
      *out = luts[selected];
      return;
    }
  }

  make_empty_lut(out);
}

// Native reimplementation of update_lut_is_valid (0x409e4): sanity-checks
// the LUT select_waveform_lut just picked - non-null pixel data, and
// positive size_kb/bit_depth/mode_width.
bool
update_lut_is_valid(const std::shared_ptr<LUTEntry>& lut) {
  auto* entry = lut.get();
  if (!entry->data)
    return false;
  if (entry->size_kb > 0 && entry->bit_depth > 0)
    return entry->mode_width > 0;
  return false;
}

void
swtcon_lock() {
#if SWTCON_32BIT_ABI_CHECK
  if (swtcon_lib_impl_enabled()) {
    qsgepaper_lock();
    return;
  }
#endif // SWTCON_32BIT_ABI_CHECK
  // LockSwapMutex: take the update-queue mutex.
  pthread_mutex_lock(&update_queue_globals()->updateQueueMutex);
}

void
swtcon_update(update_data* data) {
#if SWTCON_32BIT_ABI_CHECK
  if (swtcon_lib_impl_enabled()) {
    qsgepaper_update(data);
    return;
  }
#endif // SWTCON_32BIT_ABI_CHECK
  auto* queue = update_queue_globals();

  alignas(16) WorkItem item;
  update_item_ctor(&item); // sets seq id, empties the shared_ptrs / list

  // The input rect {y0,x0,y1,x1} is byte-reversed per 64-bit lane
  // (vrev64.32) to {x0,y0,x1,y1} before clamping.
  XYRect rev{ data->x0, data->y0, data->x1, data->y1 };
  Rect rect = clamp_update_rect(rev);
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
    item.sync = (uint8_t)((flags & Sync) != 0);
    item.fastDraw = (uint8_t)((flags & FastDraw) != 0);
    float temp = get_current_temperature();
    if (flags & ExplicitTemperature) // caller supplies an explicit temperature
      temp = (float)data->zero;
    item.temperature = temp;

    dispatch_update_regions(&item, queue->dataBuffer, queue->backBuffer);

    // Select the LUT and move the returned shared_ptr into the item -
    // assignment releases whatever was there before (empty after the ctor).
    std::shared_ptr<LUTEntry> selected;
    select_waveform_lut(
      temp, &selected, &queue->waveform, (unsigned)(int)item.mode);
    item.lut = selected;

    if (update_lut_is_valid(item.lut)) {
      auto* lut = (const int32_t*)item.lut.get();
      item.lutWidthMinus1 = (int16_t)(lut[0] - 1); // packed LUT width - 1

      // Clip this rectangle out of the pending accumulation regions and out of
      // every not-yet-locked batch already queued for the worker.
      subtract_update_region(queue->accumList, &item);
      for (auto& b : queue->listIncomingUpdates) {
        if (!b.claimed)
          subtract_update_region(b.subList, &item);
      }

      // Enqueue at the tail of the accumulation list. `item` isn't touched
      // again below, so move rather than deep-copy.
      queue->accumList.push_back(std::move(item));
    } else {
      seq = -1;
    }
  } else {
    seq = -1;
  }

  // Work-item destructor (inlined tail of queue_update): the three
  // shared_ptrs and `deps` release/free themselves automatically when
  // `item` goes out of scope below.
  (void)seq;
}

void
swtcon_unlock_post() {
#if SWTCON_32BIT_ABI_CHECK
  if (swtcon_lib_impl_enabled()) {
    qsgepaper_unlock_post();
    return;
  }
#endif // SWTCON_32BIT_ABI_CHECK
  auto* queue = update_queue_globals();

  // Find the insertion point: walk backwards from the tail, skipping
  // batches already claimed by the worker, so this batch is queued after
  // them but before any pending ones.
  auto pos = queue->listIncomingUpdates.end();
  while (pos != queue->listIncomingUpdates.begin()) {
    auto prev = std::prev(pos);
    if (!prev->claimed)
      break;
    pos = prev;
  }

  // Move the accumulated regions into a fresh batch hooked before `pos`,
  // then clear whatever's left of the (now moved-from) originals.
  build_update_batch(
    queue->listIncomingUpdates, pos, queue->accumList, queue->accumFlag);
  queue->accumList.clear();
  queue->accumFlag = 0;

  pthread_mutex_unlock(&queue->updateQueueMutex);
  sem_post(&queue->displayThreadSem); // wake the display thread
}

void
swtcon_wait() {
#if SWTCON_32BIT_ABI_CHECK
  if (swtcon_lib_impl_enabled()) {
    qsgepaper_wait();
    return;
  }
#endif // SWTCON_32BIT_ABI_CHECK
  // WaitForUpdate: spin until shutdown or the batch queue drains.
  auto* queue = update_queue_globals();
  while (queue->shutdownRequested == 0 && !queue->listIncomingUpdates.empty()) {
    usleep(100);
  }
}
