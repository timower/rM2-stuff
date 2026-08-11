#include "init.h"
#include <cstring>
#include <fstream>
#include <iostream>

LUTEntry::~LUTEntry() {
  if (data)
    free(data); // Using free() since malloc() was used
}

namespace {

// A little-endian 24-bit byte offset into the .wbf file, packed as 3 offset
// bytes followed by 1 checksum byte the library never validates. Used for
// XWIA, the mode table, and every mode/temperature table entry - the format
// masks all of them with `& 0xffffff` (FUN_00054628, load_waveform @0x458e8).
#pragma pack(push, 1)
struct Offset24 {
  uint8_t bytes[4];
  operator uint32_t() const {
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
  }
};

// The fixed 0x30-byte .wbf header (load_waveform @0x458e8 /
// FUN_00054628's raw offsets). Reserved ranges are bytes the library reads
// but this code has never needed. The per-temperature-range table
// (tempCount+1 bytes, one per index) immediately follows at offset 0x30.
struct WbfHeader {
  uint8_t reserved0[4];     // 0x00
  uint32_t fileSize;        // 0x04 - must match the actual file length
  uint8_t reserved1[6];     // 0x08
  uint16_t fplLot;          // 0x0e
  uint8_t reserved2[0xc];   // 0x10
  Offset24 xwiaOffset;      // 0x1c - offset of the length-prefixed XWIA text
  Offset24 modeTableOffset; // 0x20 - offset of the per-mode offset table
  uint8_t flags;            // 0x24 - bits [3:2] select the LUT mode width
  uint8_t modeCount;        // 0x25 - inclusive max mode index
  uint8_t tempCount;        // 0x26 - inclusive max temperature index
  uint8_t reserved3;        // 0x27
  uint8_t padValue;         // 0x28 - RLE terminator / pad byte value
  uint8_t runMarker;        // 0x29 - RLE run-mode toggle byte
  uint8_t reserved4[6];     // 0x2a
};
#pragma pack(pop)
static_assert(sizeof(Offset24) == 4, "Offset24 must match the .wbf format");
static_assert(sizeof(WbfHeader) == 0x30,
              "WbfHeader must match the .wbf format");

} // namespace

size_t
lut_data_size(const LUTEntry& lut) {
  int words_per_pixel = (7 + lut.size_kb) / 8;
  int total_words =
    lut.mode_width * lut.mode_width * words_per_pixel + words_per_pixel;
  return (size_t)total_words * 2;
}

bool
load_waveform(std::vector<ModeEntry*>* waveform_struct, const char* path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::cerr << "wfb: unable to open file: " << path << std::endl;
    return false;
  }

  file.seekg(0, std::ios::end);
  size_t sz = file.tellg();
  if (sz < sizeof(WbfHeader) + 1) {
    std::cerr << "wbf: file size too small: " << sz << std::endl;
    return false;
  }

  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> wbf(sz);
  if (!file.read((char*)wbf.data(), sz)) {
    std::cerr << "wbf: unable to read wbf file" << std::endl;
    return false;
  }

  const auto& header = *(const WbfHeader*)wbf.data();
  if (header.fileSize != sz) {
    std::cerr << "File length mismatch: " << sz << " != " << header.fileSize
              << " (hdr)" << std::endl;
    return false;
  }

  uint8_t* ptr = wbf.data();
  uint8_t mode_count = header.modeCount;
  uint8_t temp_count = header.tempCount;
  uint8_t pad_val = header.padValue;
  uint8_t run_marker = header.runMarker;
  const uint8_t* temp_table = ptr + sizeof(WbfHeader);
  const auto* mode_table =
    (const Offset24*)(ptr + (uint32_t)header.modeTableOffset);

  int mode_width = ((header.flags & 0xc) == 4) ? 0x20 : 0x10;

  const char* mode_names[] = { "INIT",       "DU",         "GC16",   "GL16",
                               "GC16_REGAL", "GL16_REGAL", "A2",     "DU4",
                               "mode8",      "mode9",      "mode10", "mode11" };

  // mode_count is an inclusive maximum index, matching load_waveform
  // @0x458e8 which loops modes 0..header.modeCount.
  for (int mode_idx = 0; mode_idx <= mode_count; mode_idx++) {
    ModeEntry* mode = new ModeEntry();
    if (mode_idx < 12)
      mode->name = mode_names[mode_idx];
    else
      mode->name = "mode" + std::to_string(mode_idx);

    for (int temp_idx = 0; temp_idx <= temp_count; temp_idx++) {
      uint32_t temp_table_offset = mode_table[mode_idx];
      const auto* wave_table = (const Offset24*)(ptr + temp_table_offset);
      uint32_t wave_offset = wave_table[temp_idx];
      uint8_t* wave_data = ptr + wave_offset;

      uint8_t first_byte = wave_data[0];
      if (first_byte == pad_val) {
        continue;
      }

      // RLE decode, mirroring FUN_00054560. run_marker is the byte that
      // toggles run mode; in run mode the byte after a value is its
      // (length-1), so the read index advances by 2 (value + length byte).
      int out_size = 0;
      int i = 0;
      bool in_run = true;
      uint8_t curr = first_byte;
      do {
        if (run_marker == curr) {
          in_run = !in_run;
        } else {
          int run;
          if (in_run) {
            i++;
            run = wave_data[i] + 1;
          } else {
            run = 1;
          }
          out_size += run * 4;
        }
        i++;
        curr = wave_data[i];
      } while (curr != pad_val);

      if (out_size == 0)
        continue;

      std::vector<uint32_t> decoded(out_size / 4);
      int out_idx = 0;
      i = 0;
      in_run = true;
      curr = wave_data[0];
      do {
        if (run_marker == curr) {
          in_run = !in_run;
        } else {
          int run;
          if (in_run) {
            i++;
            run = wave_data[i] + 1;
          } else {
            run = 1;
          }
          uint32_t val = (curr & 3) | (((curr & 0xf) >> 2) << 8) |
                         (((curr & 0x3f) >> 4) << 16) | ((curr >> 6) << 24);
          for (int j = 0; j < run; j++) {
            decoded[out_idx++] = val;
          }
        }
        i++;
        curr = wave_data[i];
      } while (curr != pad_val);

      std::shared_ptr<LUTEntry> lut = std::make_shared<LUTEntry>();
      lut->size_kb = out_size >> 10;
      lut->mode_width = mode_width;
      lut->temperature = (float)temp_table[temp_idx];
      lut->bit_depth = 2;

      int size_kb = lut->size_kb;
      size_t dest_size = lut_data_size(*lut);
      lut->data = malloc(dest_size);
      memset(lut->data, 0, dest_size);

      uint16_t* dest = (uint16_t*)lut->data;
      uint8_t* src = (uint8_t*)decoded.data();

      // Pack the decoded plane into the mode LUT. The destination column
      // index runs 0..mode_width*mode_width-1 across the whole block: for
      // source row src_row it starts at src_row*mode_width (dst_col_base)
      // and runs a full mode_width-wide span. This must match load_waveform
      // @0x458e8 exactly, otherwise the display-thread gather reads
      // out-of-range LUT indices.
      int dst_col_base = 0;
      int dst_col_end = mode_width;
      for (int src_row = 0; src_row < mode_width; src_row++) {
        int dst_col = dst_col_base;
        uint8_t* src_ptr = src + src_row;
        do {
          if (size_kb != 0) {
            uint32_t accum = 0;
            uint32_t sample_idx = 0;
            while (sample_idx < (uint32_t)size_kb) {
              uint32_t next_sample_idx = sample_idx + 1;
              uint32_t shifted_sample = (uint32_t)src_ptr[sample_idx * 0x400]
                                        << ((sample_idx & 7) * 2);
              uint16_t accum_lo16 = (uint16_t)accum;
              accum = accum | (shifted_sample & 0xffff);
              if ((next_sample_idx & 7) != 0 &&
                  (uint32_t)(size_kb - 1) != sample_idx) {
                sample_idx = next_sample_idx;
                continue;
              }
              dest[(sample_idx >> 3) * 0x401 + dst_col] =
                accum_lo16 | (uint16_t)shifted_sample;
              accum = 0;
              sample_idx = next_sample_idx;
            }
          }
          dst_col++;
          src_ptr += mode_width;
        } while (dst_col != dst_col_end);
        dst_col_base += mode_width;
        dst_col_end += mode_width;
      }

      mode->luts.push_back(lut);
    }

    waveform_struct->push_back(mode);
  }

  return true;
}

// Mirrors FUN_00054628: pulls a .wbf file's own matching fields - fpl_lot
// (u16 @ header offset 0xE) and the 2-char TFT_VID code embedded at a fixed
// offset inside the XWIA extended-info text (offset 0x1c is a 3-byte offset
// to a length-prefixed string; TFT_VID sits at that string's own offset
// 0x18). Both confirmed byte-for-byte against real .wbf files pulled off a
// device image: e.g. 320_R299_..._ED103TC2U2_...wbf has fpl_lot=299 and its
// xwia text (which is just the filename itself) slices to "U2" at that exact
// offset. Returns false only if the file is too short to even hold a
// header; a missing/oversized XWIA block just yields the "00" wildcard
// TFT_VID rather than failing outright (matches FUN_00054628, which never
// fails on XWIA content, only on a null wbf_read_file result).
bool
read_wbf_match_fields(const std::string& path,
                      uint16_t* fpl_lot,
                      char tft_vid[3]) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return false;
  file.seekg(0, std::ios::end);
  size_t sz = file.tellg();
  if (sz < sizeof(WbfHeader))
    return false;
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(sz);
  if (!file.read((char*)data.data(), sz))
    return false;

  const auto& header = *(const WbfHeader*)data.data();
  *fpl_lot = header.fplLot;

  tft_vid[0] = '0';
  tft_vid[1] = '0';
  tft_vid[2] = '\0';
  uint32_t xwia = header.xwiaOffset;
  if (xwia + 1 >= sz)
    return true;
  uint8_t xwia_len = data[xwia];
  if (xwia_len < 0x1a || xwia + 1 + xwia_len > sz)
    return true;
  const uint8_t* xwia_text = data.data() + xwia + 1;
  tft_vid[0] = (char)xwia_text[0x18];
  tft_vid[1] = (char)xwia_text[0x19];
  return true;
}
