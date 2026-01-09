#include "AppWidgets.h"

using namespace rmlib;

const MemoryCanvas&
getMissingImage() {
  static auto image = [] {
    auto mem = MemoryCanvas(128, 128, 2);
    mem.canvas.set(greyToRGB565(0xaa));
    return mem;
  }();
  return image;
}
