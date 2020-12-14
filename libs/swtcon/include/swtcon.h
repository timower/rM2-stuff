#pragma once

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* swtcon_state;

static const int SCREEN_WIDTH = 1404;
static const int SCREEN_HEIGHT = 1872;

struct Rect {
  // top-left corner
  int x1;
  int y1;

  // bottom-right corner
  int x2;
  int y2;
};

enum Waveform {
  FAST = 0,
  MEDIUM = 1,
  HQ = 2,
  INIT = HQ,
};

enum UpdateFlags {
  FullRefresh = 1,
  Sync = 2,
  FastDraw = 4, // TODO: what does this do? Used for strokes by xochitl.
};

swtcon_state
swtcon_init(const char* fb_path);
void
swtcon_destroy(swtcon_state state);

uint8_t*
swtcon_getbuffer(swtcon_state state);
void
swtcon_update(swtcon_state state, Rect rect, Waveform waveform, int flags);

void
swtcon_dump(swtcon_state state);

#ifdef __cplusplus
}
#endif
