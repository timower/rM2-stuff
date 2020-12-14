#pragma once

#include "swtcon.h"
#include <stdint.h>

namespace swtcon {

struct SwtconState {
  SwtconState(const char* path);
  ~SwtconState();

  uint8_t* getBuffer() const;
  void doUpdate(Rect rect, Waveform waveform, int flags) const;

  void dump();

private:
  const char* fbPath;
  uint8_t* imageData;
};

} // namespace swtcon
