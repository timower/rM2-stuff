#include "native_display.h"

#include <cerrno>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
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
