#include "SwtconState.h"

#include "Addresses.h"
#include "Constants.h"
#include "Waveforms.h"
#include "fb.h"

#include <string>
#include <vector>

#include <cstdlib>
#include <iostream>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>

namespace swtcon {

namespace {

constexpr int
normPhase(int i) {
  return i < 0 ? -(-i & 0xf) : i & 0xf;
}

void
notifyVsyncThread() {
  pthread_mutex_lock(vsyncMutex);
  pthread_cond_broadcast(vsyncCondVar);
  pthread_mutex_unlock(vsyncMutex);
}

void
notifyGeneratorThread() {
  pthread_mutex_lock(generatorMutex);
  *generatorNotifyVar = 0;
  pthread_cond_broadcast(generatorCondVar);
  pthread_mutex_unlock(generatorMutex);
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

      notifyGeneratorThread();
    }

    if (!*isBlanked) {
      fb::pan(0x10);
    }

    if (*previousPanPhase >= 0) {
      clearDirtyBuffer(*previousPanPhase);
      *previousPanPhase = -1;
      *dirtyClearCount += 1;
      notifyGeneratorThread();
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

int
setPriority(pthread_t thread, int priority) {
  sched_param param;
  param.sched_priority = priority;
  if (pthread_setschedparam(thread, /* SCHED_OTHER? */ 1, &param) != 0) {
    return -1;
  }

  int policy = 0;
  if (pthread_getschedparam(thread, &policy, &param) != 0 || policy != 1) {
    return -1;
  }

  return 0;
}

void
clear() {
  *vsyncClearRequest = 1;
  notifyVsyncThread();
  while (*vsyncClearRequest == 1) {
    usleep(1000);
  }
}

void
createThreads(const char* path, uint8_t* imageData) {
  *globalImageData = imageData;
  *changeTrackingBuffer = (uint8_t*)malloc(SCREEN_HEIGHT * SCREEN_WIDTH);

  memset(dirtyColumns, 0, SCREEN_WIDTH * pan_buffers_count);

  fb::fillPanBuffer(zeroBuffer, 0);

  for (int x = 1; x <= SCREEN_WIDTH; x++) {
    auto* changePtrY = &(*changeTrackingBuffer)[SCREEN_HEIGHT * SCREEN_WIDTH -
                                                x * SCREEN_HEIGHT - 1];

    auto* imagePtrY =
      &imageData[2 * SCREEN_HEIGHT * SCREEN_WIDTH - x * 2]; // 2 bytes per pixel

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      auto* changePtr = &changePtrY[y + 1];
      auto* imagePtr = &imagePtrY[-y * SCREEN_WIDTH * 2];

      uint8_t val = *imagePtr;
      val = (val & 0x1e) >> 1;
      *changePtr = val | (val << 4);
    }
  }

  if (waveform::initWaveforms() != 0) {
    std::cerr << "Error loading waveform data" << std::endl;
    std::exit(-1);
  }

  if (fb::openFb(path, pan_buffers_count) != 0) {
    std::cerr << "Error opening fb" << std::endl;
    std::exit(-1);
  }

  if (true) { // TODO
    fb::unblank(0x10);
    waveform::updateTemperature();

    if (*isBlanked == 0) {
      fb::blank();
    }
  }

  pthread_mutex_init(lastPanMutex, nullptr);
  pthread_mutex_init(vsyncMutex, nullptr);
  pthread_cond_init(vsyncCondVar, nullptr);
  if (pthread_create(vsyncThread, nullptr, /*vsyncFn*/ vsyncRoutine, nullptr) !=
      0) {
    std::cerr << "Error creating vsync thread" << std::endl;
    std::exit(-1);
  }

  if (setPriority(*vsyncThread, 99) != 0) {
    std::cerr << "Error setting vsync priority" << std::endl;
    std::exit(-1);
  }

  pthread_mutex_init(msgListmutex, nullptr);
  pthread_mutex_init(generatorMutex, nullptr);
  pthread_cond_init(generatorCondVar, nullptr);

  *generatorNotifyVar = 1;

  if (pthread_create(generatorThread, nullptr, generatorFn, nullptr) != 0) {
    std::cerr << "Error creating generator thread" << std::endl;
    std::exit(-1);
  }

  if (setPriority(*generatorThread, 98) != 0) {
    std::cerr << "Error setting generator priority" << std::endl;
    std::exit(-1);
  }
}

void
shutdown() {
  std::cout << "Shutting down swtcon threads" << std::endl;

  *generatorShutdownRequest = 1;
  notifyGeneratorThread();
  pthread_join(*generatorThread, nullptr);

  *vsyncShutdownRequest = 1;
  notifyVsyncThread();
  pthread_join(*vsyncThread, nullptr);

  waveform::freeWaveforms();
  fb::unmap();
}

} // namespace

SwtconState::SwtconState(const char* path) : fbPath(path) {
  imageData = (uint8_t*)malloc(SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(uint16_t));
  memset(imageData, 0xFF, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));

  createThreads(fbPath, imageData);
  clear();
}

SwtconState::~SwtconState() {
  shutdown();
  free(imageData);
}

uint8_t*
SwtconState::getBuffer() const {
  return imageData;
}

void
SwtconState::doUpdate(Rect rect, Waveform waveform, int flags) const {
  UpdateParams params;

  // TODO: bounds check here

  // From now on we swap x-y axis
  params.x1 = rect.y1;
  params.y1 = rect.x1;
  params.x2 = rect.y2;
  params.y2 = rect.x2;

  params.flags = flags;
  params.waveform = waveform;

  actualUpdateFn(&params);
}

void
SwtconState::dump() {
  std::cerr << "Framebuffer path: " << fbPath << std::endl;
  std::cerr << "Image address: " << std::hex << (void*)getBuffer() << std::dec
            << std::endl;
}

} // namespace swtcon
