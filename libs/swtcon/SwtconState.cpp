#include "SwtconState.h"

#include "Addresses.h"
#include "Constants.h"
#include "Generator.h"
#include "Vsync.h"
#include "Waveforms.h"
#include "fb.h"

#include <string>
#include <vector>

#include <cstdlib>
#include <iostream>
#include <string.h>
#include <unistd.h>

namespace swtcon {

namespace {
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
  vsync::notifyVsyncThread();
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
  if (pthread_create(vsyncThread, nullptr, vsync::vsyncRoutine, nullptr) != 0) {
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
  generator::notifyGeneratorThread();
  pthread_join(*generatorThread, nullptr);

  *vsyncShutdownRequest = 1;
  vsync::notifyVsyncThread();
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
