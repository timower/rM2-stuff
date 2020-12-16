#pragma once

#include <cstdint>

namespace swtcon::waveform {

struct InitWaveformInfo {
  int phases;
  uint8_t* phaseData;
};

int
initWaveforms();

void
freeWaveforms();

int
updateTemperature();

InitWaveformInfo*
getInitWaveform(int tempIdx);

} // namespace swtcon::waveform
