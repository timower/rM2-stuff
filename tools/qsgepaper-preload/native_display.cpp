#include "native_display.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "native_init.h"
#include "native_update.h"
#include "qsgepaper_globals.h"

// Native reimplementation of worker_thread_func (0x3ae38) - see AGENTS.md
// Phase 5 and swtcon_architecture.md §6.1 for the reversing history. This is
// the panel-driving frame-pacing loop: one iteration per displayed frame. It
// never touches WorkItem/dependency-list state (that's entirely
// display_thread_func's - still library code, deferred separately), so it
// stands on its own as a self-contained native port.
//
// Every step below (including the exact "y0 = curFrame - 1" bookkeeping,
// which looks like an off-by-one at first glance) was byte-verified against
// the disassembly, not just the decompiler's pseudocode - see the reversing
// notes in AGENTS.md before "simplifying" any of it.

// Stamps g_lastPanTimestamp with the current CLOCK_MONOTONIC_RAW time,
// widening this toolchain's own (possibly still 32-bit-tv_sec) struct
// timespec into the library's Y2038-safe Timespec64 field-by-field - see
// Timespec64's comment in qsgepaper_globals.h. Caller must hold
// displayTimingMutex.
static void
stamp_last_pan_timestamp(UpdateQueueGlobals* queue) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  queue->lastPanTimestamp.tv_sec = (int64_t)ts.tv_sec;
  queue->lastPanTimestamp.tv_nsec = (int64_t)ts.tv_nsec;
}

// Pans to `frame_idx`, then under displayTimingMutex stamps the timestamp
// and nLastPannedFrame (to curFrame-1, i.e. the frame we were on *before*
// this pan - matches the library exactly, see the banner above) and bumps
// curFrame. Shared by steps 7 and 10 below (the two "advance one frame"
// call sites); step 2's double pan-to-16 stamps the same fields but without
// advancing curFrame, so it isn't routed through this helper.
static void
pan_and_advance_frame(UpdateQueueGlobals* queue, FrameCursorGlobals* cursor, bool unblank) {
  int frame_idx = queue->curFrame % 16;
  if (unblank)
    native_pan_and_unblank(frame_idx);
  else
    native_pan_to_frame(frame_idx);

  pthread_mutex_lock(&queue->displayTimingMutex);
  stamp_last_pan_timestamp(queue);
  cursor->nLastPannedFrame = queue->curFrame - 1;
  queue->curFrame = queue->curFrame + 1;
  pthread_mutex_unlock(&queue->displayTimingMutex);
}

void*
native_worker_thread_func(void*) {
  auto* queue = update_queue_globals();
  auto* cursor = frame_cursor_globals();

  for (;;) {
    cursor->bWorkerThreadBusy = 1;

    // 2. Pre-frame housekeeping: if unblanked, double-pan the init slot and
    // stamp timing (no curFrame advance - the panel isn't showing a new
    // frame yet, just being kept alive).
    if (!native_is_fb_blanked()) {
      native_pan_to_frame(16);
      native_pan_to_frame(16);
      pthread_mutex_lock(&queue->displayTimingMutex);
      stamp_last_pan_timestamp(queue);
      cursor->nLastPannedFrame = queue->curFrame - 1;
      pthread_mutex_unlock(&queue->displayTimingMutex);
    }

    // 3. Wake the display thread once per worker tick.
    sem_post(&queue->displayThreadSem);

    // 4. Periodic reprime, every 60s (recomputes g_time_var fresh rather
    // than incrementing it, so a long stall doesn't cause a burst of
    // reprimes on resume).
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    double now = (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
    if ((double)(queue->timeVar + 60) <= now) {
      queue->timeVar = (int)now;
      native_prime_display();
    }

    // 5. Wait for the display thread to advance the target frame, a flash
    // request, or shutdown - bounded to 3s so a stuck target still lets us
    // blank the panel on timeout instead of waiting forever.
    pthread_mutex_lock(&queue->workerCondMutex);
    struct timespec deadline;
    bool have_deadline = clock_gettime(CLOCK_REALTIME, &deadline) == 0;
    if (have_deadline)
      deadline.tv_sec += 3;
    bool want_timed = have_deadline;

    if (queue->curFrame == queue->targetFrame) {
      for (;;) {
        if (queue->workerThreadShutdown != 0)
          break;
        if (queue->flashRequested != 0)
          break;

        if (!want_timed) {
          pthread_cond_wait(&queue->workerCond, &queue->workerCondMutex);
          if (queue->curFrame != queue->targetFrame)
            break;
          continue;
        }

        int r = pthread_cond_timedwait(&queue->workerCond, &queue->workerCondMutex, &deadline);
        if (r == ETIMEDOUT) {
          native_blank_fb();
          want_timed = false;
          if (queue->curFrame != queue->targetFrame)
            break;
          continue;
        }
        if (queue->curFrame != queue->targetFrame)
          break;
      }
    }
    pthread_mutex_unlock(&queue->workerCondMutex);

    // 6.
    cursor->bWorkerThreadBusy = 0;

    // 7. Un-blank and advance one frame, unless a flash or shutdown is
    // pending (both handled below instead).
    if (native_is_fb_blanked() && queue->workerThreadShutdown == 0 && queue->flashRequested == 0) {
      pan_and_advance_frame(queue, cursor, /*unblank=*/true);
    }

    // 8. Flash sequence: the full-panel black/white/black flash. Selects
    // the mode-0 flash waveform LUT, primes frame slots 0-2 with a
    // checkerboard pattern, walks the LUT's phase count panning through
    // each phase's pixel byte, then restores the real waveform LUT into
    // those same slots and resets per-pixel state to neutral.
    if (queue->flashRequested != 0) {
      float temp = native_get_current_temperature();
      SpRef lut_sp{};
      native_select_waveform_lut(
        temp, &lut_sp, (std::vector<ModeEntry*>*)(void*)queue->waveformStructRaw, /*mode=*/0);

      if (native_update_lut_is_valid(lut_sp)) {
        static const uint16_t kFlashPatterns[3] = { 0x0000, 0x5555, 0xaaaa };
        for (int i = 0; i < 3; i++)
          native_write_lut_pattern(native_frame_buffer_addr(i), kFlashPatterns[i]);

        auto* lut = (const LUTEntry*)lut_sp.ptr;
        native_pan_and_unblank((int)native_read_lut_packed_pixel(lut, 0, 0, 0));
        int phase_count = lut->size_kb; // doubles as this LUT's frame/phase count
        for (int phase = 1; phase < phase_count; phase++)
          native_pan_to_frame((int)native_read_lut_packed_pixel(lut, 0, 0, phase));
        native_pan_to_frame(0);
        native_pan_to_frame(16);
        native_blank_fb();
        for (int i = 0; i < 3; i++)
          native_upload_lut_to_frame_slot(native_frame_buffer_addr(i));
        native_reset_statebuffer_neutral();
      }

      release_sp(lut_sp.ctrl);
      queue->flashRequested = 0;
    }

    // 9.
    if (queue->workerThreadShutdown != 0)
      pthread_exit(nullptr);

    // 10. Catch-up: pan/advance/wake the display thread in a tight loop
    // until curFrame catches up to whatever target the display thread has
    // set.
    while (queue->curFrame < queue->targetFrame) {
      pan_and_advance_frame(queue, cursor, /*unblank=*/false);
      sem_post(&queue->displayThreadSem);
    }
  }

  return nullptr;
}

// Mirrors FUN_0003b4b4: sets flashRequested, wakes the worker thread (it
// polls this flag at step 6 of the loop above), then blocks until the flag
// clears again. The library's version does this via a futex-backed
// std::atomic<uint8_t>::wait/notify pair; a plain poll loop is equivalent
// and avoids replicating threading boilerplate nothing else reads (same
// call already made for swtcon_wait's own queue-drain spin-loop).
void
native_request_flash_and_wait() {
  auto* queue = update_queue_globals();

  queue->flashRequested = 1;
  pthread_mutex_lock(&queue->workerCondMutex);
  pthread_cond_broadcast(&queue->workerCond);
  pthread_mutex_unlock(&queue->workerCondMutex);

  while (queue->flashRequested != 0)
    usleep(100);
}

// ---------------------------------------------------------------------
// Native reimplementation of display_thread_func (0x3d2ac) and the chain it
// calls into - see swtcon_architecture.md §6.2/§6.2a/§6.4 for the
// prose write-up. Every function below was re-derived directly from Ghidra
// decompilation during this pass (not just the higher-level sketch in the
// architecture doc, which turned out to have the wrong kernel-selection
// rule for advance_work_item_frames - see that function's comment).
//
// Two pieces stay still-library, called by address exactly like
// render_update_kernel is in the update path:
//   0x50660 dispatch_processed_regions - hides a genuinely unreversed
//           rectangle-merge algorithm (own thread pool, dynamic vector
//           growth, node-based rect merging). Confirmed call signature
//           this pass: bool(ListHead* subList) - it reads a "count" field
//           at subList+2 words, which for every real caller happens to
//           alias the containing BatchNode's own `count` field, exactly
//           the same trick build_overlap_dependency_list's signature uses.
//   0x4a140/0x4a234 the two worker-side playback kernels ("plain"/
//           "overlap-aware") - call signature confirmed, bodies
//           [derived]/[guess] (see architecture doc §8).
constexpr uintptr_t kDispatchProcessedRegionsAddr = 0x50660;
constexpr uintptr_t kPlainPlaybackKernelAddr = 0x4a140;
constexpr uintptr_t kOverlapPlaybackKernelAddr = 0x4a234;

static bool
native_dispatch_processed_regions(ListHead* sub_list) {
  auto fn = resolve_ptr<bool (*)(ListHead*)>(kDispatchProcessedRegionsAddr);
  return fn(sub_list);
}

// Mirrors copy_init_frame_row (0x53be4, newly named this pass): restores
// one stale frame-slot row from the LUT blob's fixed reference row.
static void
native_copy_init_frame_row(void* frame_slot_addr, int col) {
  auto* fb = framebuffer_globals();
  uint8_t* dest = (uint8_t*)frame_slot_addr + (int64_t)(col + 3) * 0x410;
  const uint8_t* src = (const uint8_t*)fb->pLUT + 0xc30;
  memcpy(dest, src, 0x410);
}

// Mirrors display_thread_func's stale-row cleanup (§6.2 step 1, byte-exact
// per this pass's decompile): for every ring position that's fallen out of
// the live window [nFrameCleanupCursor-15, nLastPannedFrame], restore any
// column the dirty-gate array marked, then zero the whole bucket.
//
// `nLastPannedFrame` is captured once at the top, matching the library
// (it's cross-thread state the worker thread may still be advancing - the
// library reads it once into a register before the loop, not on every
// iteration). The bucket computation uses plain C `%`, which can go
// negative for a negative `i` near startup, in which case this reaches
// backwards past the array's base - a real quirk in the library itself,
// transcribed verbatim rather than "fixed" (see swtcon_architecture.md §8).
static void
native_stale_row_cleanup() {
  auto* cursor = frame_cursor_globals();
  uint8_t* dirty_gate = resolve_ptr<uint8_t*>(kBackBufferDirtyGateAddr);
  int32_t last_panned = cursor->nLastPannedFrame;

  for (int32_t i = cursor->nFrameCleanupCursor - 15; i <= last_panned; i++) {
    int32_t bucket = i % 16;
    uint8_t* row = dirty_gate + (int64_t)bucket * kDirtyGateRowBytes;
    void* frame_slot = native_frame_buffer_addr(bucket);

    for (int32_t col = 0; col < kDirtyGateRowBytes; col++) {
      if (row[col] != 0)
        native_copy_init_frame_row(frame_slot, col);
    }
    memset(row, 0, kDirtyGateRowBytes);
    cursor->nFrameCleanupCursor = i + 16;
  }
}

// Mirrors display_thread_func's g_pListProcessedUpdates GC (§6.2 step 2):
// tears down every processed item whose lifetime has expired
// (curFrame >= frameAnchor+lutWidthMinus1), first scrubbing every other
// processed item's intList of dependency entries pointing at the doomed
// item (the matching teardown for the links build_overlap_dependency_list
// builds - see §6.2a).
static void
native_gc_processed_updates() {
  auto* queue = update_queue_globals();

  auto* node = (WorkItemNode*)queue->listProcessedUpdates.next;
  while ((void*)node != &queue->listProcessedUpdates) {
    auto* next = node->next;
    WorkItem& item = node->item;

    if (queue->curFrame >= item.frameAnchor + item.lutWidthMinus1) {
      for (auto* other = (WorkItemNode*)queue->listProcessedUpdates.next;
           (void*)other != &queue->listProcessedUpdates; other = other->next) {
        WorkItem& other_item = other->item;
        for (auto* dep = (IntListNode*)other_item.intList.next; (void*)dep != &other_item.intList;) {
          auto* dep_next = dep->next;
          if ((WorkItem*)(intptr_t)dep->value == &item) {
            list_unhook(dep);
            ::operator delete(dep);
            other_item.intListCount--;
          }
          dep = dep_next;
        }
      }

      queue->processedUpdatesCount--;
      list_unhook(node);
      native_destroy_item_node(node);
    }

    node = next;
  }
}

// Mirrors build_overlap_dependency_list (0x3a838, CONFIRMED - see §6.2a):
// for each item in `sub_list`, rebuilds its intList from scratch against
// every still-active item in g_pListProcessedUpdates whose rect overlaps
// and whose lifetime outlasts this item's own frameAnchor.
static void
native_build_overlap_dependency_list(ListHead* sub_list) {
  auto* queue = update_queue_globals();

  for (auto* node = (WorkItemNode*)sub_list->next; (void*)node != sub_list; node = node->next) {
    WorkItem& item = node->item;

    for (auto* dep = (IntListNode*)item.intList.next; (void*)dep != &item.intList;) {
      auto* dep_next = dep->next;
      ::operator delete(dep);
      dep = dep_next;
    }
    item.intList = { &item.intList, &item.intList };
    item.intListCount = 0;

    if (item.rectY0 > item.rectY1 || item.rectX0 > item.rectX1)
      continue; // degenerate item rect - no dependencies possible

    for (auto* other_node = (WorkItemNode*)queue->listProcessedUpdates.next;
         (void*)other_node != &queue->listProcessedUpdates; other_node = other_node->next) {
      WorkItem& other = other_node->item;
      if (other.rectY0 > other.rectY1 || other.rectX0 > other.rectX1)
        continue; // degenerate other rect

      bool overlap = other.rectX0 <= item.rectX1 && item.rectX0 <= other.rectX1 &&
                     other.rectY0 <= item.rectY1 && item.rectY0 <= other.rectY1;
      if (!overlap)
        continue;
      if (item.frameAnchor >= other.frameAnchor + other.lutWidthMinus1)
        continue; // "other" won't outlive item - no need to track it

      auto* dep = (IntListNode*)::operator new(sizeof(IntListNode));
      dep->value = (int32_t)(intptr_t)&other; // a WorkItem*, see IntListNode's comment
      list_insert_before(&item.intList, dep);
      item.intListCount++;
    }
  }
}

// Signature shared by both still-library worker-side playback kernels
// (0x4a140/0x4a234): (frameSlots[8], item, frameCount, chunkIndex, chunkCount).
using PlaybackKernelFn = void (*)(void**, WorkItem*, int, int, int);

// Mirrors FUN_0003ec78 (0x3ec78, CONFIRMED): the third independent thread
// pool. Below 2 chunks, calls the kernel synchronously; at 2+, spins up one
// thread per chunk and joins (the library lazily maintains its own
// persistent pool here, but nothing else reads its bookkeeping globals, so
// a plain per-call std::thread is behaviorally equivalent - see
// swtcon_architecture.md §6.4). After the kernel work finishes (sync call or
// all chunks joined), commits the item's phase/frameCursor advance,
// including the LUT-wraparound correction - unsynchronized plain field
// writes, matching the library (no mutex held here either).
static void
native_playback_kernel_dispatch(PlaybackKernelFn kernel_fn, void** frame_slots, WorkItem* item,
                                 int frame_count, int chunk_count) {
  if (chunk_count < 2) {
    kernel_fn(frame_slots, item, frame_count, 0, 1);
  } else {
    std::vector<std::thread> threads;
    threads.reserve(chunk_count);
    for (int i = 0; i < chunk_count; i++)
      threads.emplace_back(kernel_fn, frame_slots, item, frame_count, i, chunk_count);
    for (auto& t : threads)
      t.join();
  }

  int32_t new_phase = item->phase + frame_count;
  int32_t lut_width = *(const int32_t*)item->lut.ptr; // full packed width, NOT lutWidthMinus1
  item->frameCursor += frame_count;
  item->phase = (int16_t)new_phase;
  if (lut_width <= new_phase) {
    int32_t excess = new_phase - lut_width;
    item->frameCursor -= excess;
    item->phase = (int16_t)(new_phase - excess);
  }
}

// Shared chunk-count rule for FUN_0003f294/FUN_0003f1f0 (CONFIRMED, a third
// distinct chunking rule from dispatch_update_regions's column-width>98 and
// dispatch_processed_regions's merged-X-span<29): 1 unless the item's rect
// is non-degenerate and its area exceeds 20000px, in which case 1 if its
// column span is under 10, else 2.
static int
native_playback_chunk_count(const WorkItem* item) {
  if (item->rectY0 <= item->rectY1 && item->rectX0 <= item->rectX1) {
    int32_t width_span = item->rectX1 - item->rectX0;
    int64_t area = (int64_t)(width_span + 1) * (item->rectY1 - item->rectY0 + 1);
    if (area > 20000)
      return width_span < 10 ? 1 : 2;
  }
  return 1;
}

// Mirrors FUN_0003f294 (0x3f294, "plain" kernel wrapper - CONFIRMED): picks
// the chunk count and dispatches through FUN_0003ec78 with the "plain"
// kernel (0x4a140).
static void
native_dispatch_plain_kernel(void** frame_slots, WorkItem* item, int frame_count) {
  auto kernel_fn = resolve_ptr<PlaybackKernelFn>(kPlainPlaybackKernelAddr);
  native_playback_kernel_dispatch(kernel_fn, frame_slots, item, frame_count,
                                   native_playback_chunk_count(item));
}

// Mirrors FUN_0003f1f0 (0x3f1f0, "overlap-aware" kernel wrapper -
// CONFIRMED): same as above but with the overlap-aware kernel (0x4a234).
static void
native_dispatch_overlap_kernel(void** frame_slots, WorkItem* item, int frame_count) {
  auto kernel_fn = resolve_ptr<PlaybackKernelFn>(kOverlapPlaybackKernelAddr);
  native_playback_kernel_dispatch(kernel_fn, frame_slots, item, frame_count,
                                   native_playback_chunk_count(item));
}

// Mirrors advance_work_item_frames (0x3a984) byte-exactly, re-derived
// directly from disassembly this pass (see swtcon_architecture.md §6.4).
// This CORRECTS the architecture doc's earlier higher-level sketch: kernel
// selection is NOT simply "intList empty -> plain, non-empty -> overlap".
// The real rule is:
//   - if intList has an ACTIVE dependency (one whose frameAnchor+
//     lutWidthMinus1 still exceeds this item's own frameAnchor): always
//     the "plain" kernel (0x4a140), regardless of phase alignment.
//   - otherwise (intList empty, or every dependency has already expired):
//     the "overlap-aware" kernel (0x4a234) if phase is 8-aligned, else
//     still the "plain" kernel.
// i.e. the overlap-aware kernel only fires on an 8-aligned phase boundary
// with no live dependency left to account for - the opposite of what its
// name suggests at a glance.
static void
native_advance_work_item_frames(WorkItem* item) {
  auto* queue = update_queue_globals();
  auto* cursor = frame_cursor_globals();

  int32_t phase = item->phase;
  int32_t phase_mod8 = phase % 8;
  int32_t remaining = (int32_t)item->lutWidthMinus1 - phase;
  int32_t start_frame_cursor = item->frameCursor;
  int32_t frame_count = 8 - phase_mod8;
  if (remaining <= frame_count)
    frame_count = remaining;

  void* frame_slots[8];
  for (int i = 0; i < 8; i++)
    frame_slots[i] = native_frame_buffer_addr((start_frame_cursor + i) % 16);

  int32_t budget = cursor->nFrameCleanupCursor - start_frame_cursor + 1;

  bool int_list_empty = (item->intList.next == &item->intList);
  bool have_active_dep = false;
  if (!int_list_empty) {
    for (auto* dep = (IntListNode*)item->intList.next; (void*)dep != &item->intList; dep = dep->next) {
      auto* other = (WorkItem*)(intptr_t)dep->value;
      if (item->frameAnchor < other->frameAnchor + other->lutWidthMinus1) {
        have_active_dep = true;
        break;
      }
    }
  }

  bool dispatched = false;
  if (have_active_dep || phase_mod8 != 0) {
    // "Plain" kernel path - when there's (or was) a dependency list to
    // consider, fold a tighter bound from every still-active, already-
    // settled (its own intList empty) linked item before clamping.
    if (!int_list_empty) {
      int32_t bound = 0x7ffffffe;
      for (auto* dep = (IntListNode*)item->intList.next; (void*)dep != &item->intList; dep = dep->next) {
        auto* other = (WorkItem*)(intptr_t)dep->value;
        bool other_settled = (other->intList.next == &other->intList) && (other->phase < other->lutWidthMinus1);
        if (other_settled)
          bound = std::min(bound, other->frameCursor - 1);
      }
      bound = bound - start_frame_cursor + 1;
      if (phase == 0 && bound <= frame_count)
        frame_count = bound;
    }

    if (frame_count == 0 || budget < frame_count)
      return; // full abort - matches the library's early `return` here (no tail either)

    native_dispatch_plain_kernel(frame_slots, item, frame_count);
    dispatched = true;
  } else if (budget >= frame_count) {
    native_dispatch_overlap_kernel(frame_slots, item, frame_count);
    dispatched = true;
  }
  // else: overlap path, insufficient budget - skip the kernel call but
  // still fall through to the shared tail below (unlike the plain path's
  // early return above).

  // Mark newly-advanced frame slots dirty in the backBuffer gate array -
  // the producer side of native_stale_row_cleanup and render_update_kernel
  // case 7's gate. Only runs if frameCursor actually moved forward (it can
  // end up <= where it started if the LUT-wraparound correction above
  // consumed the whole advance).
  if (dispatched && item->frameCursor - 1 >= start_frame_cursor) {
    uint8_t* dirty_gate = resolve_ptr<uint8_t*>(kBackBufferDirtyGateAddr);
    auto* sp3 = (const RegionRows*)item->sp3.ptr;
    for (int32_t f = start_frame_cursor; f != item->frameCursor; f++) {
      uint8_t* row = dirty_gate + (int64_t)(f % 16) * kDirtyGateRowBytes;
      for (int32_t col = sp3->x0; col <= sp3->x1; col++)
        row[col] = 1;
    }
  }

  // Shared tail: cosmetic "fallen behind" warning, then bump targetFrame and
  // wake the worker thread if this item's cursor moved it forward - the
  // only place targetFrame is written, and how the (native) worker thread's
  // wait loop learns there's more to display.
  int32_t new_frame_cursor = item->frameCursor;
  if (new_frame_cursor < queue->curFrame - 1) {
    std::cerr << "swtcon: generator thread has fallen behind... update=" << new_frame_cursor
              << ", next=" << queue->curFrame << std::endl;
  }
  if (new_frame_cursor <= queue->targetFrame)
    return;
  queue->targetFrame = new_frame_cursor;
  pthread_mutex_lock(&queue->workerCondMutex);
  pthread_cond_broadcast(&queue->workerCond);
  pthread_mutex_unlock(&queue->workerCondMutex);
}

// Mirrors display_thread_func (0x3d2ac) - see swtcon_architecture.md §6.2
// for the prose write-up of every step below (re-verified/corrected against
// fresh decompilation this pass, notably: the sync/fullRefresh gate filter
// in step 3 is now exact rather than "[derived, not fully closed]", and the
// updateQueueMutex is held across the ENTIRE intake+dispatch+commit
// sequence for every batch this tick - not dropped before the heavy
// dispatch_processed_regions call as an earlier draft of this port assumed).
void*
native_display_thread_func(void*) {
  auto* queue = update_queue_globals();
  auto* cursor = frame_cursor_globals();

  for (;;) {
    sem_wait(&queue->displayThreadSem);

    // 1. Stale-row cleanup.
    native_stale_row_cleanup();

    // 2. g_pListProcessedUpdates GC.
    native_gc_processed_updates();

    bool processed_empty = (queue->listProcessedUpdates.next == &queue->listProcessedUpdates);
    bool incoming_empty = (queue->listIncomingUpdates.next == &queue->listIncomingUpdates);
    if (processed_empty && incoming_empty && queue->shutdownRequested != 0)
      break;

    // 3-6. Incoming-batch intake, gate-check, dispatch, commit.
    if (!incoming_empty && pthread_mutex_trylock(&queue->updateQueueMutex) == 0) {
      auto* batch = (BatchNode*)queue->listIncomingUpdates.next;
      while ((void*)batch != &queue->listIncomingUpdates) {
        auto* next_batch = batch->next;

        if (batch->subList.next == &batch->subList) {
          // Empty sub-list - unhook and free immediately.
          list_unhook(batch);
          queue->incomingBatchCount--;
          native_free_update_region_list(&batch->subList);
          ::operator delete(batch);
        } else {
          native_build_overlap_dependency_list(&batch->subList);

          // Gate-check "max lifetime" scan (now exact, not
          // "[derived, not fully closed]" - a dependency is skipped only
          // when NEITHER item requested Sync AND BOTH are FullRefresh,
          // since a FullRefresh redraw makes waiting on another
          // FullRefresh-only dependency's completion pointless).
          int32_t max_lifetime = 0;
          for (auto* item_node = (WorkItemNode*)batch->subList.next;
               (void*)item_node != &batch->subList; item_node = item_node->next) {
            WorkItem& item = item_node->item;
            int32_t item_max = 0;
            for (auto* dep = (IntListNode*)item.intList.next; (void*)dep != &item.intList; dep = dep->next) {
              auto* other = (WorkItem*)(intptr_t)dep->value;
              bool skip = item.sync == 0 && other->sync == 0 && item.fullRefresh != 0 && other->fullRefresh != 0;
              if (!skip)
                item_max = std::max(item_max, other->frameAnchor + other->lutWidthMinus1);
            }
            max_lifetime = std::max(max_lifetime, item_max);
          }

          int32_t gate_target = std::max(max_lifetime, queue->curFrame);
          if (cursor->nFrameCleanupCursor - gate_target > 6) {
            bool dispatched = native_dispatch_processed_regions(&batch->subList);
            if (dispatched) {
              int32_t target;
              if (!cursor->bWorkerThreadBusy || queue->curFrame != queue->targetFrame) {
                int64_t workload_sum = 0;
                int32_t min_x0 = INT32_MAX;
                for (auto* item_node = (WorkItemNode*)batch->subList.next;
                     (void*)item_node != &batch->subList; item_node = item_node->next) {
                  WorkItem& item = item_node->item;
                  int32_t width = 0, height = 0;
                  if (item.rectY0 <= item.rectY1 && item.rectX0 <= item.rectX1) {
                    height = item.rectY1 - item.rectY0 + 1;
                    width = item.rectX1 - item.rectX0 + 1;
                  }
                  workload_sum += ((int64_t)width * height * 8) / 1000;
                  min_x0 = std::min(min_x0, item.rectX0);
                }
                int32_t budget = (int32_t)workload_sum + 100;
                int32_t pace_target = (int32_t)(((int64_t)min_x0 + 1) * 0x1d96 / 1000);

                pthread_mutex_lock(&queue->displayTimingMutex);
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC_RAW, &now);
                int32_t base_frame =
                  (queue->curFrame - 1 == cursor->nLastPannedFrame) ? queue->curFrame : queue->curFrame - 1;
                int64_t elapsed_us = (int64_t)(now.tv_sec - queue->lastPanTimestamp.tv_sec) * 1000000 +
                                     (now.tv_nsec - queue->lastPanTimestamp.tv_nsec) / 1000;
                pthread_mutex_unlock(&queue->displayTimingMutex);

                if (elapsed_us > 11762) {
                  elapsed_us = 0;
                  base_frame += 1;
                }

                int32_t diff = pace_target - (int32_t)elapsed_us;
                if (budget < diff)
                  target = base_frame;
                else if (budget - diff <= 11761)
                  target = base_frame + 1;
                else if (budget - diff <= 23523)
                  target = base_frame + 2;
                else
                  target = queue->targetFrame;
                target = std::min(target, queue->targetFrame);
              } else {
                target = queue->curFrame;
              }

              int32_t committed = std::max(max_lifetime, target);
              for (auto* item_node = (WorkItemNode*)batch->subList.next;
                   (void*)item_node != &batch->subList; item_node = item_node->next) {
                item_node->item.frameCursor = committed;
                item_node->item.frameAnchor = committed;
              }

              // Dependencies now reflect final frame numbers - rebuild.
              native_build_overlap_dependency_list(&batch->subList);

              for (auto* item_node = (WorkItemNode*)batch->subList.next;
                   (void*)item_node != &batch->subList; item_node = item_node->next) {
                native_advance_work_item_frames(&item_node->item);
                auto* new_node = (WorkItemNode*)::operator new(sizeof(WorkItemNode));
                native_update_item_copy(&new_node->item, &item_node->item);
                list_insert_before(&queue->listProcessedUpdates, new_node);
                queue->processedUpdatesCount++;
              }
            }

            list_unhook(batch);
            queue->incomingBatchCount--;
            native_free_update_region_list(&batch->subList);
            ::operator delete(batch);
          }
          // else: gate failed - leave this batch in place, retry next tick.
        }

        batch = next_batch;
      }
      pthread_mutex_unlock(&queue->updateQueueMutex);
    }

    // 7. Bottom-of-loop sweep: keep playing back any processed item that
    // still has frames left, even if this tick didn't commit a new batch.
    for (auto* node = (WorkItemNode*)queue->listProcessedUpdates.next;
         (void*)node != &queue->listProcessedUpdates; node = node->next) {
      if (node->item.phase < node->item.lutWidthMinus1)
        native_advance_work_item_frames(&node->item);
    }
  }

  return nullptr;
}
