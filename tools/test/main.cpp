#include "swtcon.h"

int
main() {

  auto* state = swtcon_init("/dev/fb0");

  swtcon_dump(state);
  Rect rect{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
  uint8_t* buffer = swtcon_getbuffer(state);
  swtcon_update(state, rect, Waveform::INIT, UpdateFlags::FullRefresh | UpdateFlags::Sync);

  swtcon_destroy(state);

  return 0;
}
