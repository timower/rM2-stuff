#include "Vsync.h"

#include "Addresses.h"
#include "Constants.h"
#include "Generator.h"
#include "Util.h"
#include "Waveforms.h"
#include "swtcon.h"

#include <iostream>
#include <string.h>

#include <sys/time.h>
#include <time.h>

namespace swtcon::vsync {

void
notifyVsyncThread() {
  pthread_mutex_lock(vsyncMutex);
  pthread_cond_broadcast(vsyncCondVar);
  pthread_mutex_unlock(vsyncMutex);
}

void
clearDirtyBuffer(int pan) {
  auto phase = normPhase(pan);

  if (false /* TODO: verify */) {
    memcpy(fb_map_ptr + phase * pan_buffer_size * pan_line_size,
           zeroBuffer,
           pan_line_size * pan_buffer_size);
    return;
  }

  auto* fbLinePtr = // Skip the 3 preamble lines
    *fb_map_ptr + phase * pan_buffer_size * pan_line_size + 3 * pan_line_size;
  auto* dirtyPtr = dirtyColumns + phase * SCREEN_WIDTH;
  const auto* endDirtyPtr = dirtyColumns + (phase + 1) * SCREEN_WIDTH;

  while (dirtyPtr != endDirtyPtr) {
    if (*dirtyPtr != 0) {
      // zero one line
      memcpy(fbLinePtr, zeroBuffer + 3 * pan_line_size, pan_line_size);
    }

    fbLinePtr += pan_line_size;
    dirtyPtr += 1;
  }

  // Set all dirty values to zero.
  memset(dirtyColumns + phase * SCREEN_WIDTH, 0, SCREEN_WIDTH);
}

void*
vsyncRoutine(void* arg) {
  while (true) {

    while (*currentPanPhase != *lastPanPhase) {
      uint32_t normPanPhase = normPhase(*currentPanPhase);
      fb::pan(normPanPhase);

      int32_t prevPhase = *previousPanPhase;
      bool positive = prevPhase >= 0;

      *previousPanPhase = *currentPanPhase;
      *currentPanPhase = *currentPanPhase + 1;

      if (positive) {
        clearDirtyBuffer(prevPhase);
        *dirtyClearCount += 1;
      }

      generator::notifyGeneratorThread();
    }

    if (!*isBlanked) {
      fb::pan(0x10);
    }

    if (*previousPanPhase >= 0) {
      clearDirtyBuffer(*previousPanPhase);
      *previousPanPhase = -1;
      *dirtyClearCount += 1;
      generator::notifyGeneratorThread();
    }

    timeval time;
    gettimeofday(&time, nullptr);
    uint32_t tempTime = (double)time.tv_sec + (double)time.tv_usec / 1000000.0;

    // Adjust to temperature every minute.
    if (tempTime - *lastTempMeasureTime > 60) {
      waveform::updateTemperature();
    }

    pthread_mutex_lock(vsyncMutex);

    timespec timeoutVal;
    bool shouldBlank;
    if (*vsyncBlankDelay == 0 ||
        clock_gettime(CLOCK_REALTIME, &timeoutVal) != 0) {
      // TODO: simplify, always sets it to false
      shouldBlank = *isBlanked;
      if (!*isBlanked) {
        fb::blank();
      } else {
        shouldBlank = false;
      }
    } else {
      timeoutVal.tv_sec += *vsyncBlankDelay;
      shouldBlank = true;
    }

    // TODO: what is this loop?
    while (*currentPanPhase == *lastPanPhase) {
      bool hadClearReq = *vsyncClearRequest;

      // If we should do something, break
      if (*vsyncShutdownRequest != 0 || *vsyncClearRequest != 0) {
        break;
      }

      // Either wait with timeout or just wait
      if (shouldBlank) {
        int reason =
          pthread_cond_timedwait(vsyncCondVar, vsyncMutex, &timeoutVal);

        bool wasBlanked = *isBlanked;

        // If we didn't timeout (got notified) or we are blanked already,
        // repeat.
        if (reason != ETIMEDOUT || (shouldBlank = hadClearReq, *isBlanked)) {
          continue;
        }

        // Otherwise, blank
        fb::blank();

        // Use timeout in next loop if we timed out and were blanked.
        shouldBlank = wasBlanked;
      } else {
        pthread_cond_wait(vsyncCondVar, vsyncMutex);
      }
    }

    pthread_mutex_unlock(vsyncMutex);

    if (*isBlanked && *vsyncShutdownRequest == 0 && *vsyncClearRequest == 0) {
      // We're blanked but don't have to clear or shutdown -> unblank.

      auto phase = normPhase(*currentPanPhase);
      fb::unblank(phase);
      *previousPanPhase = *currentPanPhase;
      *currentPanPhase += 1;
    }

    if (!*isBlanked) {
      // Move to the 'empty' phase
      fb::pan(0x10);
    }

    if (*vsyncClearRequest != 0) {
      // Do clear
      auto* clearInfo = waveform::getInitWaveform(*currentTempWaveform);
      if (clearInfo == nullptr) {
        std::cerr << "Couldn't fetch init waveform\n";
      } else {
        if (*fb_fd < 0) {
          std::cerr << "Fb device is not open\n";
        } else {
          fb::fillPanBuffer(*fb_map_ptr, 0);
          fb::fillPanBuffer(*fb_map_ptr + pan_buffer_size * pan_line_size,
                            0x5555);
          fb::fillPanBuffer(*fb_map_ptr + 2 * pan_line_size * pan_buffer_size,
                            0xaaaa);

          fb::unblank(clearInfo->phaseData[0]);
          fb::pan(0x10);
          std::cout << "Running init (" << clearInfo->phases << " phases)\n";

          for (int i = 1; i < clearInfo->phases; i++) {
            fb::pan(clearInfo->phaseData[i] & 0xf);
          }

          fb::pan(0x10);
          if (!*isBlanked) {
            fb::blank();
          }

          memcpy(*fb_map_ptr, zeroBuffer, pan_buffer_size * pan_line_size);
          memcpy(*fb_map_ptr + pan_buffer_size * pan_line_size,
                 zeroBuffer,
                 pan_buffer_size * pan_line_size);
          memcpy(*fb_map_ptr + 2 * pan_buffer_size * pan_line_size,
                 zeroBuffer,
                 pan_buffer_size * pan_line_size);
        }
      }
      *vsyncClearRequest = 0;
    }

    if (*vsyncShutdownRequest != 0) {
      pthread_exit(nullptr);
    }

  } // end while(true)
}
} // namespace swtcon::vsync
