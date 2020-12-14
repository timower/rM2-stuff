#pragma once

#include "fb.h"

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

using ActualUpdateFn = int(UpdateParams*);
using ShutdownFn = int(void);

using CreateThreadsFn = void(const char*, uint8_t*);
using ClearFn = void(void);

using FillBufferFn = void(uint8_t*, int value);
using InitWaveformsFn = int(void);
using OpenFbFn = int(const char*, int);
using UpdateTemperatureFn = void(void);
using PanAndUnblackFn = void(int);
using BlankFn = void(void);

using RountineFn = void*(void*);
using SetPriorityFn = int(pthread_t, int);

using ReadInitTableFn = int(uint8_t*);
using ReadOneFn = void(int, uint8_t*, int, bool, bool);

using ReadTempFn = int(long*);
using TempIdxFn = int(float);

// Functions:
auto* const actualUpdateFn = (ActualUpdateFn*)0x2b40c0;
auto* const shutdownFn = (ShutdownFn*)0x2b478c;

auto* const createThreadsFn = (CreateThreadsFn*)0x2b47e8;
auto* const clearFn = (ClearFn*)0x2b4024;

auto* const fillBufferFn = (FillBufferFn*)0x2c4ff8;
auto* const initWaveformsFn = (InitWaveformsFn*)0x2b8aac;
auto* const openFbFn = (OpenFbFn*)0x2b3e30;
auto* const panAndUnblackFn = (PanAndUnblackFn*)0x2b3820;
auto* const blankFn = (BlankFn*)0x2b3270;
auto* const updateTemperatureFn = (UpdateTemperatureFn*)0x2b30dc;

auto* const setPriorityFn = (SetPriorityFn*)0x2b32c4;

auto* const readInitTableFn = (ReadInitTableFn*)0x2b6fb4;
auto* const readOneWaveformFn = (ReadOneFn*)0x2b70c0;

auto* const readTemperaturFn = (ReadTempFn*)0x2b6824;
auto* const getTempIdxFn = (TempIdxFn*)0x2b7488;

// Global variables:
uint8_t** const globalImageData = (uint8_t**)0x41e824;

auto** const changeTrackingBuffer = (uint8_t**)0x41e698;
auto* const dirtyColumns = (uint8_t*)0x41e85c;
auto* const isBlanked = (bool*)0x419f00;
auto* const zeroBuffer = (uint8_t*)0x42401c;

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

auto* const vsyncFn = (RountineFn*)0x2b3918;
auto* const generatorFn = (RountineFn*)0x2b4a34;

auto* const fb_var_info = (fb::fb_var_screeninfo*)0x41e69c;
auto* const fb_fd = (int*)0x419eec;
auto* const fb_map_ptr = (void**)0x41e7f8;

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

} // namespace swtcon
