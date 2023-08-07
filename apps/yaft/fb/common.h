#pragma once

#include <FrameBuffer.h>

#include "yaft.h"

void
refresh(rmlib::fb::FrameBuffer& fb, struct terminal_t* term, bool isLandscape);
