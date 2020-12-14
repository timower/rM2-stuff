#include "swtcon.h"
#include "SwtconState.h"

extern "C" {
swtcon_state swtcon_init(const char *path) {
  auto *state = new swtcon::SwtconState(path);
  return state;
}

void swtcon_destroy(swtcon_state state) {
  auto *stateCast = static_cast<swtcon::SwtconState *>(state);
  delete stateCast;
}

uint8_t *swtcon_getbuffer(swtcon_state state) {
  const auto *stateCast = static_cast<const swtcon::SwtconState *>(state);
  return stateCast->getBuffer();
}

void swtcon_update(swtcon_state state, Rect rect, Waveform waveform,
                   int flags) {
  const auto *stateCast = static_cast<const swtcon::SwtconState *>(state);
  stateCast->doUpdate(rect, waveform, flags);
}

void swtcon_dump(swtcon_state state) {
  auto *stateCast = static_cast<swtcon::SwtconState *>(state);
  stateCast->dump();
}
}
