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

void
initUpdateMsg(UpdateMsg& msg) {
  *globalMsgCounter += 1;
  msg.msgCount = *globalMsgCounter;

  msg.info = (UpdateInfo*)calloc(1, 0x1c);
  static_assert(sizeof(UpdateInfo) == 0x1c);

  msg.info->rect = msg.rect;

  auto width = msg.rect.bottomRight.x - msg.rect.topLeft.x + 1;
  auto height = msg.rect.bottomRight.y - msg.rect.topLeft.y + 1;
  msg.info->width = width;

  msg.buffer = (uint8_t*)malloc(width * height);
  msg.info->buffer = msg.buffer;

  msg.info->refCount = 1;
}

void
setWaveforms(UpdateMsg& msg, int waveform, int tempIdx, bool fullRefresh) {
  if (!fullRefresh) {
    msg.info->waveformPtr =
      (uint8_t*)globalTempTable[waveform * 3 + tempIdx * 26 + 4];
  } else {
    msg.info->waveformPtr =
      (uint8_t*)globalTempTable[waveform * 3 + tempIdx * 26 + 3];
  }
  msg.info->waveformSize = globalTempTable[waveform * 3 + tempIdx * 26 + 2];
}

void
actualUpdate(const UpdateParams& params) {
  auto invY1 = 1403 - params.y1;
  auto invX1 = 1871 - params.x1;
  if (invY1 >= 1403) {
    invY1 = 1403;
  }

  if (invX1 >= 1871) {
    invX1 = 1871;
  }

  auto invY2 = 1403 - params.y2;
  auto invX2 = 1871 - params.x2;
  if (invY2 < 0) {
    invY2 = 0;
  }

  if (invX2 < 0) {
    invX2 = 0;
  }

  if (invX2 > 1872 || invX1 < 0 || invY2 > 1404 || invY1 < 0) {
    return;
  }

  UpdateMsg msg;
  msg.info = nullptr;
  msg.msgCount = 0;
  msg.nextUpdatePhase = 0;
  msg.someWaveformCounter = 0;
  msg.waveformCounter = 0;
  msg.unknown = false;
  msg.hasBeenCopied = 0;
  msg.buffer = 0;

  // Mirror both axis -> top left and bottom right switch.
  // TODO: figure out what the three bits do?
  msg.rect.topLeft.x = invX2 & 0xfff8;
  msg.rect.topLeft.y = invY2;
  msg.rect.bottomRight.x = invX1 | 0x7;
  msg.rect.bottomRight.y = invY1;

  if (false /* TODO */) {
    msg.rect.topLeft.y = invY2 & 0xfff8;
    msg.rect.bottomRight.y = invY1 | 0x7;
  }

  initUpdateMsg(msg);

  // Set waveform info.
  bool fullRefresh = params.flags & 0x1;
  setWaveforms(msg, params.waveform, *currentTempWaveform, fullRefresh);
  msg.info->fullRefresh = fullRefresh;
  msg.info->stroke = params.flags & 0x4;

  // Copy over from image buffer to msg buffer.
  for (short y = msg.rect.topLeft.y; y <= msg.rect.bottomRight.y; y++) {
    auto invY = 1403 - y;
    auto* imagePtr =
      ((uint16_t*)*globalImageData) + (1871 - msg.rect.topLeft.x) * 1404 + invY;
    for (short x = msg.rect.topLeft.x; x <= msg.rect.bottomRight.x;
         x++, imagePtr -= 1404) {
      auto* bufPtr = msg.buffer + msg.info->width * (y - msg.rect.topLeft.y) +
                     (x - msg.rect.topLeft.x);
      auto byte = *(uint8_t*)imagePtr;
      *bufPtr = (byte >> 1) & 0xf;
    }
  }

  pthread_mutex_lock(msgListmutex);
  if (false /* TODO */) {
    // Copy to the buffer here already.
    msg.hasBeenCopied = true;
    msg.buffer = nullptr;
  }

  if (false /* TODO: implement this */) {
  }

  globalMsgList2->push_back(msg);
  pthread_mutex_unlock(msgListmutex);
  generator::notifyGeneratorThread();
  if (params.flags & 0x1) {
    // TODO: sync. msg list 1
    while (!globalMsgList2->empty()) {
      usleep(1000);
    }
  }
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

  // actualUpdateFn(&params);
  actualUpdate(params);
}

void
SwtconState::dump() {
  std::cerr << "Framebuffer path: " << fbPath << std::endl;
  std::cerr << "Image address: " << std::hex << (void*)getBuffer() << std::dec
            << std::endl;
}

} // namespace swtcon
