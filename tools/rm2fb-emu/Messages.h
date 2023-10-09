#pragma once

#include <cstdint>

struct Input {
  int32_t x;
  int32_t y;
  int32_t type; // 1 = down, 2 = up
};

static_assert(sizeof(Input) == 3 * 4, "Input message has unexpected size");

struct Params {
  int32_t y1;
  int32_t x1;
  int32_t y2;
  int32_t x2;

  int32_t flags;
  int32_t waveform;
};
static_assert(sizeof(Params) == 6 * 4, "Params has wrong size?");
