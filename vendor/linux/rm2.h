#pragma once

#include <linux/ioctl.h>

#include "mxcfb.h"

// Custom flag for ioctl to mark the update as a raw rm2-only update.
constexpr int RM2_UPDATE_MODE = 0x42;

// Taken from KOreader
static const int WAVEFORM_MODE_INIT = 0;
static const int WAVEFORM_MODE_DU = 1;
static const int WAVEFORM_MODE_GC16 = 2;
static const int WAVEFORM_MODE_GL16 = 3;
static const int WAVEFORM_MODE_A2 = 4;

// mxcfb_update_data::flags / UpdateParams::flags bits on the
// RM2_UPDATE_MODE ioctl path - shared by rMlib's FrameBuffer.cpp (producer;
// see its rmlib::fb::UpdateFlags static_asserts) and rm2fb's IOCTL.cpp /
// ServerSwtcon.cpp / Version3.20.cpp (consumers). Bit-for-bit identical to
// swtcon.h's own UpdateFlags (prefixed so both can be visible in the same
// file without clashing) so ServerSwtcon.cpp's mapUpdate can mask straight
// through instead of translating.
enum RM2UpdateFlags {
  RM2_FLAG_NONE = 0,
  // block until the update completes (was "FullRefresh")
  RM2_FLAG_SYNC = 1 << 0,
  // skip the dependency wait against other
  // FastDraw updates (was "Priority")
  RM2_FLAG_FAST_DRAW = 1 << 1,
};

// Custom ioctl (RM2_UPDATE_MODE only) carrying a whole list of updates in
// one call - `updates`/`count` point at the caller's memory (in-process hook).
struct rm2_update_batch {
  const mxcfb_update_data* updates;
  int count;
};

#define RM2FB_SEND_UPDATES _IOW('R', 0x01, struct rm2_update_batch)
