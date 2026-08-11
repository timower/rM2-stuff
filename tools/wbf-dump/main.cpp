#include "init.h"
#include <iomanip>
#include <iostream>

int
main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: wbf-dump <path.wbf>" << std::endl;
    return 1;
  }

  std::vector<ModeEntry*> waveform;
  if (!load_waveform(&waveform, argv[1])) {
    std::cerr << "wbf-dump: failed to load " << argv[1] << std::endl;
    return 1;
  }

  std::cout << "=== " << argv[1] << ": " << waveform.size()
            << " modes ===" << std::endl;
  for (size_t mi = 0; mi < waveform.size(); mi++) {
    ModeEntry* mode = waveform[mi];
    std::cout << "mode[" << mi << "] name='" << mode->name
              << "' luts=" << mode->luts.size() << std::endl;
    for (size_t li = 0; li < mode->luts.size(); li++) {
      LUTEntry* lut = mode->luts[li].get();
      size_t len = lut_data_size(*lut);
      uint32_t sum = 2166136261u;
      if (lut->data) {
        auto* p = (const uint8_t*)lut->data;
        for (size_t b = 0; b < len; b++)
          sum = (sum ^ p[b]) * 16777619u;
      }
      // size_kb doubles as this LUT's frame/phase count (see display.cpp's
      // flash-sequence comment).
      std::cout << "  lut[" << li << "] phases=" << lut->size_kb
                << " mode_width=" << lut->mode_width
                << " temp=" << lut->temperature
                << " bit_depth=" << lut->bit_depth << " len=" << len
                << " fnv=0x" << std::hex << std::setw(8) << std::setfill('0')
                << sum << std::dec << std::endl;
    }
  }
  std::cout << "=== end wbf dump ===" << std::endl;

  return 0;
}
