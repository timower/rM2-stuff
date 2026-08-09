#include "display.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iterator>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "init.h"
#include "qsgepaper_globals.h"
#include "update.h"

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
//
// Non-static (declared in display.h) so the frame-slot-selection formula
// (queue->curFrame % kFrameSlotRingCount) can be unit-tested directly,
// rather than only indirectly through a real, racy worker-thread run.
void
pan_and_advance_frame(UpdateQueueGlobals* queue,
                      FrameCursorGlobals* cursor,
                      bool unblank) {
  int frame_idx = queue->curFrame % kFrameSlotRingCount;
  if (unblank)
    pan_and_unblank(frame_idx);
  else
    pan_to_frame(frame_idx);

  pthread_mutex_lock(&queue->displayTimingMutex);
  stamp_last_pan_timestamp(queue);
  cursor->nLastPannedFrame = queue->curFrame - 1;
  queue->curFrame = queue->curFrame + 1;
  pthread_mutex_unlock(&queue->displayTimingMutex);
}

// swtcon_suspend/swtcon_resume's gate - see swtcon.h for the full
// rationale. Rides on queue->workerCond/workerCondMutex (the same condvar
// steps 1 and 5 already share below) rather than a condvar of its own:
// swtcon_suspend()/swtcon_resume() are documented (swtcon.h) as only valid
// after swtcon_init() has returned, so there's no need for this to outlive
// that condvar's own init/shutdown lifecycle. The flag itself stays atomic
// so is_worker_suspended() can be polled from inside step 5's loop while
// workerCondMutex is already held there, without relocking it.
namespace {
std::atomic<bool> g_workerSuspended{ false };

// Peeks the suspend flag without blocking - used by worker_thread_func at
// points other than the main gate (step 1) to notice a suspend request
// that arrived while it was blocked elsewhere in the loop. See
// swtcon_suspend()'s comment for why the gate alone isn't sufficient.
bool
is_worker_suspended() {
  return g_workerSuspended.load(std::memory_order_relaxed);
}
} // namespace

void
swtcon_suspend() {
  // curFrame==targetFrame alone is a transient snapshot - step 7 re-bumps it
  // for items still mid-waveform, so also wait for listProcessedUpdates.
  auto* queue = update_queue_globals();
  while (queue->workerThreadShutdown == 0 &&
         (queue->curFrame != queue->targetFrame ||
          !queue->listProcessedUpdates.empty()))
    usleep(100);

  g_workerSuspended.store(true, std::memory_order_relaxed);

  // The gate at the top of worker_thread_func's outer loop (step 1) is the
  // only place that actually blocks on g_workerSuspended - but the worker
  // thread spends most of its time NOT there, blocked instead inside step
  // 5's wait for the next frame to advance to. Setting the flag alone
  // doesn't wake it from that wait; if left alone, the next real update
  // (or the loop's own 3s idle timeout) would eventually wake it anyway,
  // and it would then run the rest of that iteration - including the real
  // pan_and_advance_frame()/flash hardware calls in steps 7-8 - before
  // ever looping back to check this flag. Confirmed empirically via
  // tools/qsgepaper-preload/suspend_sync_probe.cpp: a Sync update
  // submitted right after swtcon_suspend() still produced a real
  // FBIOPAN/FBIOBLANK sequence before returning. Broadcasting workerCond
  // here kicks the worker thread out of that wait immediately so the new
  // break condition added to that loop (and the re-check right after it)
  // can catch the suspend request before any hardware-touching step runs.
  pthread_mutex_lock(&queue->workerCondMutex);
  pthread_cond_broadcast(&queue->workerCond);
  pthread_mutex_unlock(&queue->workerCondMutex);
}

// See display.h - only wakes the gate, doesn't touch nIsFbBlanked.
void
wake_suspend_gate() {
  g_workerSuspended.store(false, std::memory_order_relaxed);
  auto* queue = update_queue_globals();
  pthread_mutex_lock(&queue->workerCondMutex);
  pthread_cond_broadcast(&queue->workerCond);
  pthread_mutex_unlock(&queue->workerCondMutex);
}

void
swtcon_resume() {
  // Force this instance's own nIsFbBlanked tracking to "blanked" before
  // waking the worker thread, regardless of what it actually was when we
  // suspended. It goes stale while suspended - whatever else drove the
  // panel in the meantime (e.g. xochitl's own, independent swtcon
  // instance, coordinated only via SIGSTOP/SIGCONT, not shared state) has
  // no way to keep it in sync. Reporting "blanked" makes the worker
  // thread's very next pan go through pan_and_unblank's real
  // FBIOBLANK-unblank sequence instead of a bare pan_to_frame/
  // pan_and_advance_frame call - confirmed on real hardware: a bare pan to
  // an actually-blanked/powered-down panel is rejected by the driver
  // ("Pan failed"), repeatedly, since nothing ever unblanks it. Re-
  // unblanking an already-unblanked panel is a harmless no-op at the
  // driver level, so this is safe even when the panel was never actually
  // blanked while we were suspended.
  //
  // Only correct here because the worker thread is about to resume real,
  // ongoing per-frame operation (steps 2/7/8/10 below) that will actually
  // observe and act on this flag. swtcon_shutdown() (swtcon.cpp) also wakes
  // the gate but must NOT go through this force - it does so with a plain
  // workerCond broadcast instead, since exiting doesn't care about
  // nIsFbBlanked's staleness.
  framebuffer_globals()->nIsFbBlanked = 1;
  wake_suspend_gate();
}

void*
worker_thread_func(void*) {
  auto* queue = update_queue_globals();
  auto* cursor = frame_cursor_globals();

  for (;;) {
    // Suspend gate: while suspended, block here doing nothing at all - no
    // housekeeping, no reprime, no pan - see swtcon_suspend's comment in
    // swtcon.h. Checked first, before anything below touches hardware.
    // Shares workerCond/workerCondMutex with step 5 below - a spurious
    // wakeup from some unrelated broadcast (new work, flash, shutdown) just
    // re-checks this loop's own predicate and goes back to waiting if still
    // suspended, same as any other condvar user.
    pthread_mutex_lock(&queue->workerCondMutex);
    while (g_workerSuspended.load(std::memory_order_relaxed) &&
           queue->workerThreadShutdown == 0)
      pthread_cond_wait(&queue->workerCond, &queue->workerCondMutex);
    pthread_mutex_unlock(&queue->workerCondMutex);

    if (queue->workerThreadShutdown != 0)
      pthread_exit(nullptr);

    cursor->bWorkerThreadBusy = 1;

    // 2. Pre-frame housekeeping: if unblanked, double-pan the init slot and
    // stamp timing (no curFrame advance - the panel isn't showing a new
    // frame yet, just being kept alive).
    if (!is_fb_blanked()) {
      pan_to_frame(kInitFrameSlotIndex);
      pan_to_frame(kInitFrameSlotIndex);
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
      prime_display();
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
        // Not part of the byte-verified original algorithm (see this
        // function's banner comment) - swtcon_suspend()/swtcon_resume()
        // are this project's own addition, with no analog in the real
        // library. Bail out of the wait as soon as a suspend is
        // requested, same as the shutdown/flash checks above, so the
        // re-check right after this loop (below) can skip steps 7-8
        // instead of running one more real pan first.
        if (is_worker_suspended())
          break;

        if (!want_timed) {
          pthread_cond_wait(&queue->workerCond, &queue->workerCondMutex);
          if (queue->curFrame != queue->targetFrame)
            break;
          continue;
        }

        int r = pthread_cond_timedwait(
          &queue->workerCond, &queue->workerCondMutex, &deadline);
        if (r == ETIMEDOUT) {
          blank_fb();
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

    // Re-check suspend immediately after the wait above returns - not
    // part of the original algorithm, see the break condition added to
    // that loop above for why. A suspend request may have arrived while
    // we were blocked in it; skip straight back to the top of the outer
    // loop (which blocks properly on the real gate, step 1) instead of
    // running steps 7-8's hardware-touching work first. Shutdown still
    // takes priority (falls through to step 9 below as before) - only
    // skip ahead here if we're suspended and NOT shutting down.
    if (queue->workerThreadShutdown == 0 && is_worker_suspended())
      continue;

    // 7. Un-blank and advance one frame, unless a flash or shutdown is
    // pending (both handled below instead).
    if (is_fb_blanked() && queue->workerThreadShutdown == 0 &&
        queue->flashRequested == 0) {
      pan_and_advance_frame(queue, cursor, /*unblank=*/true);
    }

    // 8. Flash sequence: the full-panel black/white/black flash. Selects
    // the mode-0 flash waveform LUT, primes frame slots 0-2 with a
    // checkerboard pattern, walks the LUT's phase count panning through
    // each phase's pixel byte, then restores the real waveform LUT into
    // those same slots and resets per-pixel state to neutral.
    if (queue->flashRequested != 0) {
      float temp = get_current_temperature();
      std::shared_ptr<LUTEntry> lut_sp;
      select_waveform_lut(temp,
                          &lut_sp,
                          &queue->waveform,
                          /*mode=*/0);

      if (update_lut_is_valid(lut_sp)) {
        static const uint16_t kFlashPatterns[3] = { 0x0000, 0x5555, 0xaaaa };
        for (int i = 0; i < 3; i++)
          write_lut_pattern(frame_buffer_addr(i), kFlashPatterns[i]);

        auto* lut = lut_sp.get();
        pan_and_unblank((int)read_lut_packed_pixel(lut, 0, 0, 0));
        int phase_count =
          lut->size_kb; // doubles as this LUT's frame/phase count
        for (int phase = 1; phase < phase_count; phase++)
          pan_to_frame((int)read_lut_packed_pixel(lut, 0, 0, phase));
        pan_to_frame(0);
        pan_to_frame(kInitFrameSlotIndex);
        blank_fb();
        for (int i = 0; i < 3; i++)
          upload_lut_to_frame_slot(frame_buffer_addr(i));
        reset_statebuffer_neutral();
      }

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
request_flash_and_wait() {
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
// calls into - see swtcon_architecture.md §6.2/§6.2a/§6.3/§6.4 for the
// prose write-up. Every function below was re-derived directly from Ghidra
// decompilation during this pass (not just the higher-level sketch in the
// architecture doc, which turned out to have the wrong kernel-selection
// rule for advance_work_item_frames - see that function's comment).
//
// Both worker-side playback kernels (0x4a140 "plain" and 0x4a234, formerly
// mislabeled "overlap-aware" - see dispatch_aligned_kernel's comment
// for why that name was wrong) are native now, sharing one implementation
// (playback_kernel_plain_intrinsics below) - see dispatch_aligned_kernel's
// comment for why a single native function covers both by-address
// originals. Zero by-address calls remain in the production pipeline; the
// library is only ever touched at all under SWTCON_LIBIMPL (swtcon.cpp),
// which switches every public swtcon_* entry point to the real library
// wholesale rather than splicing it into individual native call sites.

extern void* g_pStateBufferNative;

// Native reimplementation of the display-commit kernels FUN_0004f8f0
// (item.sync==0, "incremental") / FUN_0004e680 (sync!=0, "force") - see
// swtcon_architecture.md §6.3 for the confirmed per-pixel formula (derived
// from FUN_0004e680's disassembly, which has no narrowing logic at all
// unlike FUN_0004f8f0, then cross-checked byte-for-byte via
// dispatch_processed_regions_probe.cpp reading back the real library's
// g_pStateBuffer/item.pixelTransitions contents). Runs over the item's FULL
// rect in one pass rather than the library's 1-2 column chunks: chunking
// only splits this same per-pixel computation into disjoint column ranges
// with no shared mutable state across chunks, so it's provably invisible to
// the output (same reasoning as render_update_kernel's chunking,
// confirmed empirically here too - probe experiment 7).
//
// Also allocates item.pixelTransitions fresh (releasing whatever it held
// before) - this is that field's actual allocation site (previously
// unconfirmed).
//
// Returns true if the item survives (rect updated in place to the
// committed/narrowed result); false if it ends up degenerate ("incremental"
// with literally nothing changed anywhere) and should be destroyed by the
// caller exactly like an already-degenerate item.
//
// Non-static (declared in display.h) for direct unit testing, like the rest
// of this file's un-staticed functions below - none are per-pixel paths.
bool
commit_item(WorkItem* item) {
  int rows = item->rectY1 - item->rectY0 + 1;
  int cols = item->rectX1 - item->rectX0 + 1;
  int strideRows = (rows + kRegionRowsStrideAlign - 1) &
                   ~(kRegionRowsStrideAlign - 1); // round_up(rows, 16)

  auto* rr = new RegionRows{};
  rr->y0 = item->rectY0;
  rr->x0 = item->rectX0;
  rr->y1 = item->rectY1;
  rr->x1 = item->rectX1;
  rr->stride = strideRows;
  rr->size = strideRows * cols * (int)sizeof(uint16_t);
  auto* transitionBuf = new uint16_t[(size_t)strideRows * cols];
  rr->dataPtr = (uint8_t*)transitionBuf;

  // RegionRows::dataPtr is a plain non-owning pointer (see its comment in
  // update.h) - this allocation actually owns the uint16_t buffer it
  // just made, so free it (correctly typed) via a custom deleter on the
  // shared_ptr rather than giving RegionRows itself an owning destructor.
  item->pixelTransitions = std::shared_ptr<RegionRows>(rr, [](RegionRows* p) {
    delete[] (uint16_t*)p->dataPtr;
    delete p;
  });
  // Cache the pixelTransitions buffer into item.transitionDataPtr (+0x44).
  // This is NOT optional bookkeeping: the still-library playback kernels
  // FUN_0004a140/FUN_0004a234 read their per-pixel transitions through
  // item+0x44 directly, not via pixelTransitions->dataPtr - so leaving it
  // stale is what made wiring this function in SIGSEGV downstream.
  //
  // Crucially, +0x44 is NOT the pixelTransitions buffer BASE - it's that base
  // REBASED to the item's (post-narrowing) rect origin within the buffer,
  // exactly like `pixelDataPtr` rebases regionRows->dataPtr. The
  // pixelTransitions buffer is allocated for the seed rect (clamped outward
  // to the 8-row NEON group, so its y0/x0 can sit below the item's real
  // y0/x0); the incremental path below then narrows item->rect to the
  // changed-pixel bbox. The playback kernels iterate the NARROWED rect and
  // index +0x44 with narrowed-relative coords, so +0x44 must skip the
  // stride*(rectX0-pixelTransitions.x0)+(rectY0-pixelTransitions.y0)
  // elements between the buffer origin and the narrowed rect. We seed it to
  // the base here (correct for the force path, which never narrows, and for
  // unsplit items where rect==pixelTransitions rect); the incremental path
  // recomputes it after narrowing. Empirically verified: the real library's
  // +0x44 for a narrowed item was pixelTransitions.dataPtr + 0x670, matching
  // stride*(804-803)+(1072-1064) = 824 u16 = 0x670 (SWTCON_FRAME_LOG A/B).
  item->transitionDataPtr = rr->dataPtr;

  // Read the new pixel through item.pixelDataPtr (the REBASED
  // RegionRows::dataPtr, +0x08), NOT regionRows->dataPtr. For a split piece
  // (piece_builder) the regionRows shared_ptr still points at the
  // ORIGINAL region's struct (origin = old rect's x0/y0), and only
  // `pixelDataPtr` is rebased to the piece's own origin - so indexing
  // regionRows->dataPtr with piece-local coords reads from the wrong spot
  // and leaves border seams. FUN_0004f8f0 uses param_1[2] (==
  // item.pixelDataPtr) as the base with stride from regionRows->stride,
  // exactly this. (For a whole, unsplit item pixelDataPtr ==
  // regionRows->dataPtr, which is why full-screen updates rendered fine
  // before this fix.)
  auto* regionRows = item->regionRows.get();
  const uint8_t* pixelBuf =
    regionRows ? (const uint8_t*)item->pixelDataPtr : nullptr;
  int pixelStride = regionRows ? regionRows->stride : 0;
  uint16_t* state = (uint16_t*)g_pStateBufferNative;
  bool force = item->sync != 0;

  if (force) {
    // "Force" never narrows (confirmed: FUN_0004e680 has no min/max update
    // code at all - out_rect stays exactly the original seeded rect), so
    // there's nothing to track here - just run every pixel unconditionally.
    for (int col = item->rectX0; col <= item->rectX1; col++) {
      int localCol = col - item->rectX0;
      for (int row = item->rectY0; row <= item->rectY1; row++) {
        int localRow = row - item->rectY0;
        uint16_t new_raw =
          pixelBuf ? pixelBuf[(int64_t)localCol * pixelStride + localRow] : 0;
        uint16_t old = state[(size_t)col * kScreenHeight + row];
        bool is_sentinel = new_raw == kGatedPixelSentinel;
        uint16_t effective_new = is_sentinel ? old : new_raw;
        uint16_t packed =
          is_sentinel
            ? kPixelTransitionUnchanged
            : (uint16_t)((old << kPixelTransitionShift) | effective_new);
        transitionBuf[(size_t)strideRows * localCol + localRow] = packed;
        state[(size_t)col * kScreenHeight + row] = effective_new;
      }
    }
    return true;
  }

  // "Incremental": bounds seed at the OPPOSITE extremes from the rect
  // (Y0/X0 seeded high, Y1/X1 seeded low) so they only ever narrow inward as
  // real changes are found - matches the library's own min/max-from-
  // opposite-ends seeding (§6.3). A fully-unchanged item leaves them
  // crossed (outY0>outY1), which the degenerate check below then catches.
  int32_t outY0 = item->rectY1, outY1 = item->rectY0;
  int32_t outX0 = item->rectX1, outX1 = item->rectX0;

  for (int col = item->rectX0; col <= item->rectX1; col++) {
    int localCol = col - item->rectX0;
    // Process 8 rows (one NEON lane group) at a time, matching the
    // library's own grouping - required to reproduce its group-granular
    // g_pStateBuffer/out_rect update gating exactly (a lone changed pixel
    // still widens Y to its whole enclosing group of 8, confirmed via
    // dispatch_processed_regions_probe.cpp's single-pixel test).
    for (int groupStart = item->rectY0; groupStart <= item->rectY1;
         groupStart += 8) {
      int groupEnd = std::min(groupStart + 7, item->rectY1);
      bool groupChanged = false;
      uint16_t newState[8], packedVal[8];

      for (int row = groupStart; row <= groupEnd; row++) {
        int localRow = row - item->rectY0;
        int lane = row - groupStart;
        uint16_t new_raw =
          pixelBuf ? pixelBuf[(int64_t)localCol * pixelStride + localRow] : 0;
        uint16_t old = state[(size_t)col * kScreenHeight + row];
        bool is_sentinel = new_raw == kGatedPixelSentinel;
        bool unchanged = new_raw == old;
        bool skip = is_sentinel || unchanged;
        uint16_t effective_new = skip ? old : new_raw;

        newState[lane] = effective_new;
        packedVal[lane] =
          skip ? kPixelTransitionUnchanged
               : (uint16_t)((old << kPixelTransitionShift) | effective_new);
        transitionBuf[(size_t)strideRows * localCol + localRow] =
          packedVal[lane];
        if (!skip)
          groupChanged = true;
      }

      if (groupChanged) {
        // Group-granular: write back every row in the group (a no-op value
        // for its own unchanged lanes) and widen Y to the WHOLE group.
        for (int row = groupStart; row <= groupEnd; row++)
          state[(size_t)col * kScreenHeight + row] = newState[row - groupStart];
        outY0 = std::min(outY0, groupStart);
        outY1 = std::max(outY1, groupEnd);
        outX0 = std::min(outX0, col);
        outX1 = std::max(outX1, col);
      }
    }
  }

  if (outY0 > outY1 || outX0 > outX1)
    return false;
  item->rectY0 = outY0;
  item->rectY1 = outY1;
  item->rectX0 = outX0;
  item->rectX1 = outX1;
  // Rebase +0x44 to the narrowed rect's origin within the (un-narrowed)
  // pixelTransitions buffer - see the seeding comment above. rr->{x0,y0}
  // still hold the seed origin the buffer was laid out with; transitionBuf
  // is indexed with strideRows*(col-seedX0)+(row-seedY0), so the narrowed
  // rect starts at strideRows*(outX0-seedX0)+(outY0-seedY0) into it.
  item->transitionDataPtr = transitionBuf +
                            (size_t)strideRows * (outX0 - rr->x0) +
                            (size_t)(outY0 - rr->y0);
  return true;
}

// Native reimplementation of dispatch_processed_regions (0x50660) - see
// swtcon_architecture.md §6.2 step 4 for the reversing history (an earlier
// pass mistook the per-item chunk bookkeeping for a genuine cross-item
// rectangle merge; empirically disproved via
// tools/qsgepaper-preload/dispatch_processed_regions_probe.cpp - items in a
// batch are processed completely independently). Runs each item through
// commit_item in a single full-rect pass (no thread pool - see that
// function's comment for why chunking is provably invisible here), destroys
// items that come back degenerate (either already-degenerate on entry, or
// "incremental" with literally nothing changed) via a plain std::list erase,
// and returns true iff the sub-list is non-empty afterward.
//
// WIRED IN (this pass). The per-item algorithm is independently verified
// byte-exact against the real library (dispatch_processed_regions_probe.cpp,
// 7 passing experiments incl. pixelTransitions' exact (old<<5)|new packing
// and 0x0400 marker).
//
// The "integration hazard" that previously kept this by-address - a
// deterministic SIGSEGV inside the still-library playback kernel
// FUN_0004a234 a few calls downstream - was a stale WorkItem.transitionDataPtr
// (+0x44), NOT anything about FUN_0004a234 itself: the real
// dispatch_processed_regions caches pixelTransitions.ptr->dataPtr into
// item+0x44 right after allocating pixelTransitions (`node[0x13] =
// *piVar21`), and BOTH playback kernels (FUN_0004a140/FUN_0004a234) read
// their per-pixel transitions through that cached +0x44 pointer, not via
// pixelTransitions.ptr->dataPtr. commit_item now sets it (see there);
// without it the kernels dereferenced a stale/null pointer. That also
// explains why the earlier elimination log missed it: the "downstream state
// identical" check enumerated frameCursor/frameAnchor/phase/rect/sync/
// lutWidthMinus1 but not +0x44, and zero-filling pixelTransitions' PAYLOAD
// couldn't help since the kernels never read pixelTransitions.ptr->dataPtr.
bool
dispatch_processed_regions_native(std::list<WorkItem>& sub_list) {
  bool any_survived = false;
  for (auto it = sub_list.begin(); it != sub_list.end();) {
    WorkItem& item = *it;

    bool degenerate_on_entry =
      item.rectY1 < item.rectY0 || item.rectX1 < item.rectX0;
    bool survives = !degenerate_on_entry && commit_item(&item);

    if (!survives) {
      it = sub_list.erase(it);
    } else {
      any_survived = true;
      ++it;
    }
  }
  return any_survived;
}

// Mirrors copy_init_frame_row (0x53be4, newly named this pass): restores
// one stale frame-slot row from the LUT blob's fixed reference row.
static void
copy_init_frame_row(void* frame_slot_addr, int col) {
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
void
stale_row_cleanup() {
  auto* cursor = frame_cursor_globals();
  uint8_t* dirty_gate = backbuffer_dirty_gate();
  int32_t last_panned = cursor->nLastPannedFrame;

  for (int32_t i = cursor->nFrameCleanupCursor - (kFrameSlotRingCount - 1);
       i <= last_panned;
       i++) {
    int32_t bucket = i % kFrameSlotRingCount;
    uint8_t* row = dirty_gate + (int64_t)bucket * kDirtyGateRowBytes;
    void* frame_slot = frame_buffer_addr(bucket);

    for (int32_t col = 0; col < kDirtyGateRowBytes; col++) {
      if (row[col] != 0)
        copy_init_frame_row(frame_slot, col);
    }
    memset(row, 0, kDirtyGateRowBytes);
    cursor->nFrameCleanupCursor = i + kFrameSlotRingCount;
  }
}

// Mirrors display_thread_func's g_pListProcessedUpdates GC (§6.2 step 2):
// tears down every processed item whose lifetime has expired
// (curFrame >= frameAnchor+lutWidthMinus1), first scrubbing every other
// processed item's deps of entries pointing at the doomed item (the
// matching teardown for the links build_overlap_dependency_list builds -
// see §6.2a).
void
gc_processed_updates() {
  auto* queue = update_queue_globals();

  for (auto it = queue->listProcessedUpdates.begin();
       it != queue->listProcessedUpdates.end();) {
    WorkItem& item = *it;

    if (queue->curFrame >= item.frameAnchor + item.lutWidthMinus1) {
      for (auto& other : queue->listProcessedUpdates) {
        auto& deps = other.deps;
        deps.erase(std::remove(deps.begin(), deps.end(), &item), deps.end());
      }

      it = queue->listProcessedUpdates.erase(it);
    } else {
      ++it;
    }
  }
}

// Mirrors build_overlap_dependency_list (0x3a838, CONFIRMED - see §6.2a):
// for each item in `sub_list`, rebuilds its deps from scratch against every
// still-active item in g_pListProcessedUpdates whose rect overlaps and
// whose lifetime outlasts this item's own frameAnchor.
void
build_overlap_dependency_list(std::list<WorkItem>& sub_list) {
  auto* queue = update_queue_globals();

  for (auto& item : sub_list) {
    item.deps.clear();

    if (item.rectY0 > item.rectY1 || item.rectX0 > item.rectX1)
      continue; // degenerate item rect - no dependencies possible

    for (auto& other : queue->listProcessedUpdates) {
      if (other.rectY0 > other.rectY1 || other.rectX0 > other.rectX1)
        continue; // degenerate other rect

      bool overlap = other.rectX0 <= item.rectX1 &&
                     item.rectX0 <= other.rectX1 &&
                     other.rectY0 <= item.rectY1 && item.rectY0 <= other.rectY1;
      if (!overlap)
        continue;
      if (item.frameAnchor >= other.frameAnchor + other.lutWidthMinus1)
        continue; // "other" won't outlive item - no need to track it

      item.deps.push_back(&other);
    }
  }
}

// Signature shared by both worker-side playback kernels (native
// playback_kernel_plain_intrinsics and the still-library FUN_0004a234):
// (frameSlots[8], item, frameCount, chunkIndex, chunkCount).
using PlaybackKernelFn = void (*)(void**, WorkItem*, int, int, int);

// playback_kernel_plain_intrinsics itself (the native port of FUN_0004a140/
// FUN_0004a234, "the plain playback kernel") now lives in its own
// translation unit, playback_kernel_intrinsics.cpp, so its NEON fast path
// (Phase 9, see AGENTS.md) can sit behind an #ifdef without dragging in
// this file's threading/globals dependencies. See that file's header
// comment for the full byte-verified algorithm breakdown.

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
playback_kernel_dispatch(PlaybackKernelFn kernel_fn,
                         void** frame_slots,
                         WorkItem* item,
                         int frame_count,
                         int chunk_count) {
  if (chunk_count < 2) {
    kernel_fn(frame_slots, item, frame_count, 0, 1);
  } else {
    std::vector<std::thread> threads;
    threads.reserve(chunk_count);
    for (int i = 0; i < chunk_count; i++)
      threads.emplace_back(
        kernel_fn, frame_slots, item, frame_count, i, chunk_count);
    for (auto& t : threads)
      t.join();
  }

  int32_t new_phase = item->phase + frame_count;
  int32_t lut_width =
    *(const int32_t*)item->lut.get(); // full packed width, NOT lutWidthMinus1
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
int
playback_chunk_count(const WorkItem* item) {
  if (item->rectY0 <= item->rectY1 && item->rectX0 <= item->rectX1) {
    int32_t width_span = item->rectX1 - item->rectX0;
    int64_t area =
      (int64_t)(width_span + 1) * (item->rectY1 - item->rectY0 + 1);
    if (area > 20000)
      return width_span < 10 ? 1 : 2;
  }
  return 1;
}

// Mirrors FUN_0003f294 (0x3f294, "plain" kernel wrapper - CONFIRMED): picks
// the chunk count and dispatches through FUN_0003ec78 with the "plain"
// kernel - playback_kernel_plain (playback_kernel.s), a direct assembly
// transliteration of the real FUN_0004a140, not by-address anymore and not
// the from-scratch NEON-intrinsics playback_kernel_plain_intrinsics either (see
// AGENTS.md's Phase 9 "fifth pass": the intrinsics port passed every
// black-box verification this codebase has - probe, ab-test, bench - but
// still produced visible real-hardware artifacts; the literal
// transliteration fixed them, root cause still unidentified).
static void
dispatch_plain_kernel(void** frame_slots, WorkItem* item, int frame_count) {
  int chunk_count = playback_chunk_count(item);
  playback_kernel_dispatch(playback_kernel_plain_intrinsics,
                           frame_slots,
                           item,
                           frame_count,
                           chunk_count);
}

// Mirrors FUN_0003f1f0 (0x3f1f0, the "aligned" kernel wrapper - CONFIRMED):
// same as above, dispatching to what used to be the separate by-address
// kernel at 0x4a234.
//
// NAMING: this and FUN_0004a234 were previously called "overlap-aware"/
// "overlap" - a name inherited from the fact that the selection rule below
// (advance_work_item_frames) consults the item's overlap-dependency
// intList to choose between this path and the plain one. That name is
// backwards and misleading: this path fires exactly when there is NO active
// overlap dependency left to account for (see have_active_dep below) - it
// has nothing to do with overlap handling itself. The real distinguishing
// requirement is `phase % 8 == 0` (see FUN_0004a234's NEON strategy below) -
// an alignment-driven fast path, not an overlap-driven one. Renamed to
// "aligned" throughout (function, address macros, log tag) to match.
//
// FUN_0004a234 turned out NOT to be a distinct algorithm: a decisive probe
// test (tools/qsgepaper-preload/playback_kernel_probe.cpp Experiment 7) calls
// both FUN_0004a140 and the real FUN_0004a234 on an IDENTICAL WorkItem/state/
// LUT (a rich, pseudo-random, non-uniform fill spanning 4 columns x 16 rows)
// for every frameCount 1-8 (including the three cases - 1,2,3 - that
// FUN_0004a234 itself tail-calls out to separate, still fully unreversed
// delegates FUN_0004a3f8/FUN_0004a9e0/FUN_0004b098 for) and diffs every byte
// of the output: all eight cases came back byte-for-byte IDENTICAL. A Ghidra
// decompile of FUN_0004a234's case 8 (the most common in practice - see
// below) independently corroborates this: it touches the exact same
// WorkItem fields as FUN_0004a140 (0xc/0x10/0x14/0x18/0x28/0x2c/0x3c/0x44),
// the same LUT-index formula, the same destination address formula, and the
// same row->bit packing order - no new field, no global, no `intList`
// access, no allocation, no threads. The only difference is NEON unrolling
// strategy: FUN_0004a234's cases 4-8 extract all 8 sub-phases from one
// lane-shifted vector at once (valid only because the aligned-kernel
// selection rule - see advance_work_item_frames's kernel-selection comment -
// guarantees `phase&7==0` whenever this path is taken, i.e. there's no
// `(phase&7)+k` offset to compute), where FUN_0004a140 does the general
// `(phase&7)+k` slice; cases 1-3's delegates presumably do the analogous
// thing for a phase-aligned start with a short remaining run. Different
// code, provably same output - so no separate native implementation is
// needed, and the delegates never need reversing at all.
static void
dispatch_aligned_kernel(void** frame_slots, WorkItem* item, int frame_count) {
  int chunk_count = playback_chunk_count(item);
  playback_kernel_dispatch(playback_kernel_aligned_intrinsics,
                           frame_slots,
                           item,
                           frame_count,
                           chunk_count);
}

// Mirrors advance_work_item_frames (0x3a984) byte-exactly, re-derived
// directly from disassembly this pass (see swtcon_architecture.md §6.4).
// This CORRECTS the architecture doc's earlier higher-level sketch: kernel
// selection is NOT simply "intList empty -> plain, non-empty -> aligned".
// The real rule is:
//   - if intList has an ACTIVE dependency (one whose frameAnchor+
//     lutWidthMinus1 still exceeds this item's own frameAnchor): always
//     the "plain" kernel (0x4a140), regardless of phase alignment.
//   - otherwise (intList empty, or every dependency has already expired):
//     the "aligned" kernel (0x4a234) if phase is 8-aligned, else still the
//     "plain" kernel.
// i.e. the aligned kernel only fires on an 8-aligned phase boundary with no
// live dependency left to account for - which is exactly why "overlap-aware"
// (its name before this pass) was backwards and misleading: it fires when
// there's NO overlap left to track, not when there is. It's an alignment
// fast path, not an overlap-aware one - see dispatch_aligned_kernel's
// comment.
void
advance_work_item_frames(WorkItem* item) {
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
    frame_slots[i] =
      frame_buffer_addr((start_frame_cursor + i) % kFrameSlotRingCount);

  int32_t budget = cursor->nFrameCleanupCursor - start_frame_cursor + 1;

  bool int_list_empty = item->deps.empty();
  bool have_active_dep = false;
  for (auto* other : item->deps) {
    if (item->frameAnchor < other->frameAnchor + other->lutWidthMinus1) {
      have_active_dep = true;
      break;
    }
  }

  bool dispatched = false;
  if (have_active_dep || phase_mod8 != 0) {
    // "Plain" kernel path - when there's (or was) a dependency list to
    // consider, fold a tighter bound from every still-active, already-
    // settled (its own deps empty) linked item before clamping.
    if (!int_list_empty) {
      int32_t bound = 0x7ffffffe;
      for (auto* other : item->deps) {
        bool other_settled =
          other->deps.empty() && (other->phase < other->lutWidthMinus1);
        if (other_settled)
          bound = std::min(bound, other->frameCursor - 1);
      }
      bound = bound - start_frame_cursor + 1;
      if (phase == 0 && bound <= frame_count)
        frame_count = bound;
    }

    if (frame_count == 0 || budget < frame_count)
      return; // full abort - matches the library's early `return` here (no tail
              // either)

    dispatch_plain_kernel(frame_slots, item, frame_count);
    dispatched = true;
  } else if (budget >= frame_count) {
    dispatch_aligned_kernel(frame_slots, item, frame_count);
    dispatched = true;
  }
  // else: aligned path, insufficient budget - skip the kernel call but
  // still fall through to the shared tail below (unlike the plain path's
  // early return above).

  // Mark newly-advanced frame slots dirty in the backBuffer gate array -
  // the producer side of stale_row_cleanup and render_update_kernel
  // case 7's gate. Only runs if frameCursor actually moved forward (it can
  // end up <= where it started if the LUT-wraparound correction above
  // consumed the whole advance).
  if (dispatched && item->frameCursor - 1 >= start_frame_cursor) {
    uint8_t* dirty_gate = backbuffer_dirty_gate();
    auto* transitions = item->pixelTransitions.get();
    for (int32_t f = start_frame_cursor; f != item->frameCursor; f++) {
      uint8_t* row =
        dirty_gate + (int64_t)(f % kFrameSlotRingCount) * kDirtyGateRowBytes;
      for (int32_t col = transitions->x0; col <= transitions->x1; col++)
        row[col] = 1;
    }
  }

  // Shared tail: cosmetic "fallen behind" warning, then bump targetFrame and
  // wake the worker thread if this item's cursor moved it forward - the
  // only place targetFrame is written, and how the (native) worker thread's
  // wait loop learns there's more to display.
  int32_t new_frame_cursor = item->frameCursor;
  if (new_frame_cursor < queue->curFrame - 1) {
    std::cerr << "swtcon: generator thread has fallen behind... update="
              << new_frame_cursor << ", next=" << queue->curFrame << std::endl;
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
// fresh decompilation this pass, notably: the sync/fastDraw gate filter
// in step 3 is now exact rather than "[derived, not fully closed]", and the
// updateQueueMutex is held across the ENTIRE intake+dispatch+commit
// sequence for every batch this tick - not dropped before the heavy
// dispatch_processed_regions call as an earlier draft of this port assumed).
void*
display_thread_func(void*) {
  auto* queue = update_queue_globals();
  auto* cursor = frame_cursor_globals();

  for (;;) {
    sem_wait(&queue->displayThreadSem);

    // 1. Stale-row cleanup.
    stale_row_cleanup();

    // 2. g_pListProcessedUpdates GC.
    gc_processed_updates();

    bool processed_empty = queue->listProcessedUpdates.empty();
    bool incoming_empty = queue->listIncomingUpdates.empty();
    if (processed_empty && incoming_empty && queue->shutdownRequested != 0)
      break;

    // 3-6. Incoming-batch intake, gate-check, dispatch, commit.
    if (!incoming_empty &&
        pthread_mutex_trylock(&queue->updateQueueMutex) == 0) {
      for (auto batch_it = queue->listIncomingUpdates.begin();
           batch_it != queue->listIncomingUpdates.end();) {
        if (batch_it->subList.empty()) {
          // Empty sub-list - erase immediately.
          batch_it = queue->listIncomingUpdates.erase(batch_it);
          continue;
        }

        Batch& batch = *batch_it;
        build_overlap_dependency_list(batch.subList);

        // Gate-check "max lifetime" scan (exact condition, not
        // "[derived, not fully closed]" - a dependency is skipped exactly
        // when NEITHER item requested Sync AND BOTH have FastDraw set. This
        // bit was misnamed "FullRefresh" until this pass (see swtcon.h's
        // UpdateFlags comment for the full evidence trail); the old
        // "a full refresh redraws the whole area anyway" rationale for this
        // skip condition doesn't hold under the corrected name and isn't
        // replaced with a new confirmed one here - the condition itself is
        // exact (byte-verified against the disassembly), but WHY it's
        // specifically "both FastDraw" that skips the dependency wait is an
        // open question, not an established fact.
        int32_t max_lifetime = 0;
        for (auto& item : batch.subList) {
          int32_t item_max = 0;
          for (auto* other : item.deps) {
            bool skip = item.sync == 0 && other->sync == 0 &&
                        item.fastDraw != 0 && other->fastDraw != 0;
            if (!skip)
              item_max =
                std::max(item_max, other->frameAnchor + other->lutWidthMinus1);
          }
          max_lifetime = std::max(max_lifetime, item_max);
        }

        int32_t gate_target = std::max(max_lifetime, queue->curFrame);
        if (cursor->nFrameCleanupCursor - gate_target > 6) {
          bool dispatched = dispatch_processed_regions_native(batch.subList);

          if (dispatched) {
            int32_t target;
            if (!cursor->bWorkerThreadBusy ||
                queue->curFrame != queue->targetFrame) {
              int64_t workload_sum = 0;
              int32_t min_x0 = INT32_MAX;
              for (auto& item : batch.subList) {
                int32_t width = 0, height = 0;
                if (item.rectY0 <= item.rectY1 && item.rectX0 <= item.rectX1) {
                  height = item.rectY1 - item.rectY0 + 1;
                  width = item.rectX1 - item.rectX0 + 1;
                }
                workload_sum += ((int64_t)width * height * 8) / 1000;
                min_x0 = std::min(min_x0, item.rectX0);
              }
              int32_t budget = (int32_t)workload_sum + 100;
              int32_t pace_target =
                (int32_t)(((int64_t)min_x0 + 1) * kColumnPaceUsPerColumnX1000 /
                          1000);

              pthread_mutex_lock(&queue->displayTimingMutex);
              struct timespec now;
              clock_gettime(CLOCK_MONOTONIC_RAW, &now);
              int32_t base_frame =
                (queue->curFrame - 1 == cursor->nLastPannedFrame)
                  ? queue->curFrame
                  : queue->curFrame - 1;
              int64_t elapsed_us =
                (int64_t)(now.tv_sec - queue->lastPanTimestamp.tv_sec) *
                  1000000 +
                (now.tv_nsec - queue->lastPanTimestamp.tv_nsec) / 1000;
              pthread_mutex_unlock(&queue->displayTimingMutex);

              if (elapsed_us > (int64_t)kPanelFrameTickUs + 1) {
                elapsed_us = 0;
                base_frame += 1;
              }

              int32_t diff = pace_target - (int32_t)elapsed_us;
              if (budget < diff)
                target = base_frame;
              else if (budget - diff <= (int32_t)kPanelFrameTickUs)
                target = base_frame + 1;
              else if (budget - diff <= (int32_t)kPanelFrameTickUs * 2 + 1)
                target = base_frame + 2;
              else
                target = queue->targetFrame;
              target = std::min(target, queue->targetFrame);
            } else {
              target = queue->curFrame;
            }

            int32_t committed = std::max(max_lifetime, target);
            for (auto& item : batch.subList) {
              item.frameCursor = committed;
              item.frameAnchor = committed;
            }

            // Dependencies now reflect final frame numbers - rebuild.
            build_overlap_dependency_list(batch.subList);

            // Move each surviving item onward into the processed list -
            // batch.subList's own copies are moved-from (empty) afterward,
            // so erasing the whole batch below is cheap and doesn't need a
            // separate free_update_region_list-style pass.
            for (auto& item : batch.subList) {
              advance_work_item_frames(&item);
              queue->listProcessedUpdates.push_back(std::move(item));
            }
          }

          batch_it = queue->listIncomingUpdates.erase(batch_it);
        } else {
          // gate failed - leave this batch in place, retry next tick.
          ++batch_it;
        }
      }
      pthread_mutex_unlock(&queue->updateQueueMutex);
    }

    // 7. Bottom-of-loop sweep: keep playing back any processed item that
    // still has frames left, even if this tick didn't commit a new batch.
    for (auto& item : queue->listProcessedUpdates) {
      if (item.phase < item.lutWidthMinus1)
        advance_work_item_frames(&item);
    }
  }

  return nullptr;
}
