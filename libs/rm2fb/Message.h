#pragma once

#include <iostream>

struct UpdateParams {
  int y1;
  int x1;
  int y2;
  int x2;

  int flags;
  int waveform;
};

inline std::ostream&
operator<<(std::ostream& stream, const UpdateParams& msg) {
  return stream << "{ { " << msg.x1 << ", " << msg.y1 << "; " << msg.x2 << ", "
                << msg.y2 << " }, wave: " << msg.waveform
                << " flags: " << msg.flags << " }";
}
