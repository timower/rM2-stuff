#pragma once

#include "fb.h"

#include <atomic>
#include <list>
#include <pthread.h>
#include <stdint.h>
#include <string>

namespace swtcon {

struct UpdateParams {
  int x1;
  int y1;
  int x2;
  int y2;
  int flags;
  int waveform;
};

struct ShortPoint {
  // Top 3 bits are used for something?
  short x;
  short y;
};

struct ShortRect {
  ShortPoint topLeft;
  ShortPoint bottomRight;
};

struct UpdateInfo {
  uint8_t* buffer;
  ShortRect rect;

  short width;

  short refCount;

  uint8_t* waveformPtr;
  short waveformIdx;
  short waveformSize;
  bool fullRefresh;
  bool stroke;

  bool unknown1;
  bool unknown2;
};

struct UpdateMsg {
  UpdateInfo* info;
  int msgCount;
  int nextUpdatePhase;
  int someWaveformCounter;

  ShortRect rect;

  short waveformCounter;
  bool unknown;
  bool hasBeenCopied;

  uint8_t* buffer;
};

using ActualUpdateFn = int(UpdateParams*);
using RountineFn = void*(void*);

// Functions:
auto* const actualUpdateFn = (ActualUpdateFn*)0x2b40c0;
auto* const generatorFn = (RountineFn*)0x2b4a34;

// Global variables:
uint8_t** const globalImageData = (uint8_t**)0x41e824;

auto** const changeTrackingBuffer = (uint8_t**)0x41e698;
auto* const dirtyColumns = (uint8_t*)0x41e85c;
auto* const isBlanked = (bool*)0x419f00;
auto* const zeroBuffer = (uint8_t*)0x42401c;

auto* const fb_var_info = (fb::fb_var_screeninfo*)0x41e69c;
auto* const fb_fd = (int*)0x419eec;
auto* const fb_map_ptr = (uint8_t**)0x41e7f8;

auto* const globalTempTable = (uint32_t*)0x5898ac;
auto* const globalInitTable = (uint32_t*)0x58983c;

auto* const generatorShutdownRequest = (uint32_t*)0x41e850;
auto* const vsyncClearRequest = (uint32_t*)0x41e81c;
auto* const vsyncShutdownRequest = (uint32_t*)0x41e818;

auto* const lastTempMeasureTime = (uint32_t*)0x41e814;
auto* const currentTemperature = (float*)0x041e810;
auto* const currentTempWaveform = (uint32_t*)0x419ef8;
auto* const tempPath = (std::string*)0x058981c;
auto* const haveTempPath = (bool*)0x589834;

auto* const currentPanPhase = (int*)0x41e7fc;
auto* const lastPanPhase = (int*)0x41e800;
auto* const previousPanPhase = (int*)0x419f08;

static_assert(sizeof(std::atomic_int) == 4);
auto* const dirtyClearCount = (std::atomic_int*)0x419ef4;

auto* const vsyncBlankDelay = (int*)0x0419f10;

auto* const lastPanSec = (int*)0x41e754;
auto* const lastPanNsec = (int*)0x41e758;

// Threads stuff:
auto* const lastPanMutex = (pthread_mutex_t*)0x41e73c;
auto* const vsyncMutex = (pthread_mutex_t*)0x41e75c;
auto* const vsyncCondVar = (pthread_cond_t*)0x41e778;

auto* const vsyncThread = (pthread_t*)0x41e858;

auto* const msgListmutex = (pthread_mutex_t*)0x41e828;
auto* const generatorMutex = (pthread_mutex_t*)0x41e7a8;
auto* const generatorCondVar = (pthread_cond_t*)0x41e7c8;

auto* const generatorThread = (pthread_t*)0x41e854;

auto* const generatorNotifyVar = (int*)0x41e7c0;

auto* const globalMsgCounter = (uint32_t*)0x589838;

auto* const globalMsgList2 = (std::list<UpdateMsg>*)0x41e844;

} // namespace swtcon
