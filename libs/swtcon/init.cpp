#include "init.h"
#include "statebuffer_table.h"
#include "update.h" // swtcon_state()
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/fb.h>
#include <mutex>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// The library keeps four separate buffers (see init_statebuffer + the
// qsgepaper_init prologue). They are distinct allocations with distinct fills,
// all held directly in SwtconState now (update.h) rather than as loose
// globals here:
//   queue->dataBuffer            -> g_pDataBuffer  @0x670bc, kStatebufferSize,
//   memset 0xff
//                                   (the 16-bit image buffer returned to the
//                                   caller)
//   queue->backBuffer            -> g_pBackBuffer  @0x670c0,
//   kScreenWidth*kScreenHeight,
//                                   calloc (full-screen 1 byte/pixel back
//                                   buffer)
//   statebuffer->pStatebuffer    -> DAT_0006d1d0   @0x6d1d0, kStatebufferSize,
//   memset 0x1e
//                                   (the persisted statebuffer)
//   statebuffer->pGammaTable     -> DAT_0006d1d4   @0x6d1d4, kGammaTableSize
//   ('U' + gamma
//                                   LUT, 128 temperature entries x 0x88 bytes)
//                                   read by the render kernel FUN_0004e7b8 as
//                                   uVar40*0x88 + base.

LUTEntry::~LUTEntry() {
  if (data)
    free(data); // Using free() since malloc() was used
}

int
create_pid_file() {
  int fd = open("/tmp/epd.lock", O_RDWR | O_CREAT, 0666);
  if (fd == -1) {
    std::cerr << "unable to open lock file: /tmp/epd.lock" << std::endl;
    return -1;
  }
  if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
    swtcon_state()->nPidFd = fd;
    return 0;
  }
  std::cerr << "another instance is already running" << std::endl;
  close(fd);
  return -1;
}

int
init_statebuffer(void* dataBuffer, void* backBuffer) {
  size_t sz = kStatebufferSize;
  auto* queue = update_queue_globals();
  auto* sb = statebuffer_globals();

  // g_pDataBuffer: 16-bit image working buffer, returned to the caller.
  queue->dataBufferOwned = dataBuffer == nullptr;
  queue->dataBuffer = dataBuffer == nullptr ? malloc(sz) : dataBuffer;
  if (!queue->dataBuffer)
    return -1;
  memset(queue->dataBuffer, 0xff, sz);

  // g_pBackBuffer: full-screen 1 byte/pixel back buffer.
  queue->backBufferOwned = backBuffer == nullptr;
  queue->backBuffer = backBuffer == nullptr
                        ? calloc((size_t)kScreenWidth * kScreenHeight, 1)
                        : backBuffer;
  if (!queue->backBuffer)
    return -1;

  // DAT_0006d1d0: the persisted statebuffer. init_statebuffer @0x4fad4 fills it
  // with the 32-bit pattern kNeutralStateWord (bytes 1e 00 1e 00) — i.e. a
  // per-pixel uint16 state of 0x001e, NOT every byte = 0x1e. Using a plain
  // memset(0x1e) makes each state 0x1e1e and corrupts the waveform
  // transitions the render kernels compute.
  sb->pStatebuffer = malloc(sz);
  if (!sb->pStatebuffer)
    return -1;
  {
    uint32_t* sp = (uint32_t*)sb->pStatebuffer;
    for (size_t i = 0; i < sz / 4; i++)
      sp[i] = kNeutralStateWord;
  }

  // DAT_0006d1d4: 'U' followed by the gamma LUT (128 entries x 0x88 bytes).
  sb->pGammaTable = malloc(kGammaTableSize);
  if (!sb->pGammaTable)
    return -1;
  // Matches init_statebuffer @0x4fad4: values are treated as UNSIGNED 16-bit
  // (the library does VectorSignedToFloat((uint)*puVar6) on a zero-extended
  // ushort). statebuffer_table is pre-shifted to start at the library's first
  // read (Ghidra 0x596da), so index i maps directly.
  char* pc = (char*)sb->pGammaTable;
  *pc++ = 'U';
  for (int i = 0; i < 17407; i++) {
    double d = (double)statebuffer_table[i];
    d = round((d * 124.0) / 65532.0);
    *pc++ = (d > 0.0) ? (char)d : 0;
  }

  sb->nSize = kStatebufferSize;

  return 0;
}

namespace {

// The fb_var_screeninfo fields init_framebuffer (@0x53c0c) itself sets,
// shared with init_framebuffer_with_fd below.
void
fill_var_screeninfo(struct fb_var_screeninfo& vinfo,
                    const FbInitParams& fb_info) {
  vinfo.xres = fb_info.xres;
  vinfo.yres = fb_info.yres;
  vinfo.xres_virtual = fb_info.xres;
  vinfo.yres_virtual = fb_info.yres * (fb_info.frameCount + 1);
  vinfo.yoffset = fb_info.frameCount * fb_info.yres;
  vinfo.bits_per_pixel = fb_info.bitsPerPixel;
  vinfo.pixclock = fb_info.pixclock;
  vinfo.left_margin = fb_info.leftMargin;
  vinfo.right_margin = fb_info.rightMargin;
  vinfo.upper_margin = fb_info.upperMargin;
  vinfo.lower_margin = fb_info.lowerMargin;
  vinfo.hsync_len = fb_info.hsyncLen;
  vinfo.vsync_len = fb_info.vsyncLen;
}

// One full frame in bytes (bpp*xres*yres/8), shared by both init paths below.
unsigned int
frame_bytes_of(const FbInitParams& fb_info) {
  return (unsigned int)(fb_info.bitsPerPixel * fb_info.xres * fb_info.yres) >>
         3;
}

} // namespace

int
init_framebuffer(const FbInitParams& fb_info) {
  const char* file = "/dev/fb0";
  auto* fb = framebuffer_globals();
  fb->nFbFd = open(file, O_RDWR);
  if (fb->nFbFd < 0) {
    std::cerr << "Cannot open device" << std::endl;
    return -1;
  }

  // Fill the module-global finfo/vinfo (not locals): the display/pan code reads
  // fb->fbVar directly, so the full struct must persist past init.
  struct fb_fix_screeninfo& finfo = fb->fbFix;
  struct fb_var_screeninfo& vinfo = fb->fbVar;
  memset(&finfo, 0, sizeof(finfo));
  if (ioctl(fb->nFbFd, FBIOGET_FSCREENINFO, &finfo) == -1) {
    std::cerr << "Error reading fixed information" << std::endl;
    return -1;
  }

  // Read the current mode, then overwrite exactly the fields init_framebuffer
  // (@0x53c0c) sets, leaving everything else as the driver reported it.
  memset(&vinfo, 0, sizeof(vinfo));
  if (ioctl(fb->nFbFd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
    std::cerr << "Unable to read screeninfo" << std::endl;
    return -1;
  }

  fill_var_screeninfo(vinfo, fb_info);

  if (ioctl(fb->nFbFd, FBIOPUT_VSCREENINFO, &vinfo) == -1) {
    std::cerr << "Error writing variable information" << std::endl;
    return -1;
  }

  int y_factor = (char)(fb_info.frameCount + 1);
  unsigned int frame_bytes = frame_bytes_of(fb_info);
  size_t size = (size_t)y_factor * frame_bytes;

  fb->pFbMmap =
    mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->nFbFd, 0);
  if (fb->pFbMmap == MAP_FAILED) {
    std::cerr << "Error: failed to map framebuffer device to memory"
              << std::endl;
    return -1;
  }

  fb->nFbSizeX = frame_bytes;
  fb->nFbSizeY = y_factor;

  return 0;
}

void
init_framebuffer_with_fd(const FbInitParams& fb_info, int fd) {
  auto* fb = framebuffer_globals();
  memset(&fb->fbFix, 0, sizeof(fb->fbFix));
  struct fb_var_screeninfo& vinfo = fb->fbVar;
  memset(&vinfo, 0, sizeof(vinfo));
  fill_var_screeninfo(vinfo, fb_info);

  int y_factor = (char)(fb_info.frameCount + 1);
  unsigned int frame_bytes = frame_bytes_of(fb_info);
  size_t size = (size_t)y_factor * frame_bytes;

  // A real fd (memfd_create() or a temp file), just not a framebuffer
  // device - ftruncate it to size so mmap has something to map.
  ftruncate(fd, size);
  fb->nFbFd = fd;
  fb->pFbMmap = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  fb->nFbSizeX = frame_bytes;
  fb->nFbSizeY = y_factor;
}

// Mirrors FUN_00053a30 exactly (constants read from its disassembly — the
// vector decompilation of the immediates is misleading). vmov/vorr .i32
// immediates are byte-position encoded: #0x410000 == 0x00410000, #0x20000 ==
// 0x00020000, etc.
void
init_lut_sub(uint32_t* param_1, int param_2) {
  uint32_t* puVar3 = param_1;
  uint32_t* puVar5 = param_1 + 0x104;
  while (puVar3 != puVar5) {
    puVar3[0] = 0x410000;
    puVar3[1] = 0x410000;
    puVar3[2] = 0x410000;
    puVar3[3] = 0x410000;
    puVar3 += 4;
  }

  uint32_t* p = param_1 + 7;
  do {
    p++;
    *p |= 0x200000;
  } while (p != param_1 + 18);

  uint32_t* puVar4 = param_1 + 55;
  do {
    puVar4[0] |= 0x20000;
    puVar4[1] |= 0x20000;
    puVar4[2] |= 0x20000;
    puVar4[3] |= 0x20000;
    puVar4 += 4;
  } while (puVar4 != param_1 + 255);

  if (param_2 == -1)
    return;

  uint32_t* p2 = param_1 + 26;
  while (p2 != puVar5) {
    p2[0] |= 0x100000;
    p2[1] |= 0x100000;
    p2 += 2;
  }
  uint32_t* p3 = param_1 + 26;
  while (p3 != puVar5) {
    p3[0] |= param_2;
    p3[1] |= param_2;
    p3 += 2;
  }
}

// Mirrors FUN_00053ac4 exactly: fills a 0x165800-byte LUT-shaped blob (one
// full frame slot's worth) with the fixed dither pattern, parameterized only
// by the third init_lut_sub call's pattern argument - `init_lut`
// passes 0 there (the real waveform LUT); the worker thread's flash sequence
// (write_flash_prime_pattern @0x53c04, wrapping this same function) passes a
// dynamic 16-bit pattern instead. Shared so both callers can't drift apart.
static void
fill_lut_pattern(void* dest, int pattern) {
  uint32_t* buf = (uint32_t*)dest;

  // vmov.i32 #0x430000 == 0x00430000 (byte-position immediate, not 0x43000000).
  for (int i = 0; i < 0x104; i++) {
    buf[i] = 0x430000;
  }
  // Both loops store the final element before the exit compare, so the upper
  // bound is inclusive: words 0x14..0x8e and 0x28..0x66 (FUN_00053ac4 disasm).
  for (int i = 0x14; i <= 0x8e; i++) {
    buf[i] |= 0x40000;
  }
  for (int i = 0x28; i <= 0x66; i++) {
    buf[i] &= 0xfffdffff;
  }

  // The middle call passes -1: in FUN_00053ac4 r1 is set to 0xffffffff before
  // the first call and NOT reloaded before the second; only the third uses the
  // real param.
  init_lut_sub(buf + 0x104, -1);
  init_lut_sub(buf + 0x208, -1);
  init_lut_sub(buf + 0x30c, pattern);

  uint32_t* puVar2 = buf + 0x410;
  while (puVar2 != buf + 0x59600) {
    memcpy(puVar2, buf + 0x30c, 0x410); // 0x410 bytes = 0x104 words
    puVar2 += 0x104;
  }
}

int
init_lut() {
  size_t sz = kLutBlobSize;
  auto* fb = framebuffer_globals();
  fb->pLUT = malloc(sz);
  if (!fb->pLUT)
    return -1;
  fill_lut_pattern(fb->pLUT, 0);
  return 0;
}

// Mirrors write_flash_prime_pattern (0x53c04, wraps FUN_00053ac4): writes the
// same dither-fill algorithm as init_lut, but into an already-existing
// frame-slot-sized buffer and with a caller-supplied pattern rather than the
// real waveform LUT's fixed 0. Used by the worker thread's flash sequence to
// prime frame slots 0-2 with a checkerboard pattern.
void
write_lut_pattern(void* dest, int pattern) {
  fill_lut_pattern(dest, pattern);
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
  if (sz < 0x31) {
    std::cerr << "wbf: file size too small: " << sz << std::endl;
    return false;
  }

  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> wbf(sz);
  if (!file.read((char*)wbf.data(), sz)) {
    std::cerr << "wbf: unable to read wbf file" << std::endl;
    return false;
  }

  uint32_t hdr_sz = *(uint32_t*)(wbf.data() + 4);
  if (hdr_sz != sz) {
    std::cerr << "File length mismatch: " << sz << " != " << hdr_sz << " (hdr)"
              << std::endl;
    return false;
  }

  uint8_t* ptr = wbf.data();
  uint8_t mode_count = ptr[0x25];
  uint8_t temp_count = ptr[0x26];
  uint8_t pad_val = ptr[0x28];
  uint32_t mode_table_offset = *(uint32_t*)(ptr + 0x20) & 0xffffff;

  int iVar27 = ((ptr[0x24] & 0xc) == 4) ? 0x20 : 0x10;

  const char* mode_names[] = { "INIT",       "DU",         "GC16",   "GL16",
                               "GC16_REGAL", "GL16_REGAL", "A2",     "DU4",
                               "mode8",      "mode9",      "mode10", "mode11" };

  // mode_count (ptr[0x25]) is an inclusive maximum index, matching
  // load_waveform
  // @0x458e8 which loops modes 0..ptr[0x25].
  for (int mode_idx = 0; mode_idx <= mode_count; mode_idx++) {
    ModeEntry* mode = new ModeEntry();
    if (mode_idx < 12)
      mode->name = mode_names[mode_idx];
    else
      mode->name = "mode" + std::to_string(mode_idx);

    for (int temp_idx = 0; temp_idx <= temp_count; temp_idx++) {
      uint32_t mode_table = mode_table_offset;
      uint32_t temp_table_offset =
        *(uint32_t*)(ptr + mode_table + mode_idx * 4) & 0xffffff;
      uint32_t wave_offset =
        *(uint32_t*)(ptr + temp_table_offset + temp_idx * 4) & 0xffffff;
      uint8_t* wave_data = ptr + wave_offset;

      uint8_t uVar8 = wave_data[0];
      if (uVar8 == pad_val) {
        continue;
      }

      // RLE decode, mirroring FUN_00054560. ptr[0x29] is the run marker
      // that toggles run mode; in run mode the byte after a value is its
      // (length-1), so the read index advances by 2 (value + length byte).
      int out_size = 0;
      int i = 0;
      bool bVar1 = true;
      uint8_t curr = uVar8;
      do {
        if (ptr[0x29] == curr) {
          bVar1 = !bVar1;
        } else {
          int run;
          if (bVar1) {
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
      bVar1 = true;
      curr = wave_data[0];
      do {
        if (ptr[0x29] == curr) {
          bVar1 = !bVar1;
        } else {
          int run;
          if (bVar1) {
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
      lut->mode_width = iVar27;
      lut->temperature = (float)ptr[0x30 + temp_idx];
      lut->bit_depth = 2;

      int uVar14 = out_size >> 10;
      int uVar3 = (7 + uVar14) / 8;
      int iVar5 = iVar27 * iVar27 * uVar3 + uVar3;
      int dest_size = iVar5 * 2;
      lut->data = malloc(dest_size);
      memset(lut->data, 0, dest_size);

      uint16_t* dest = (uint16_t*)lut->data;
      uint8_t* src = (uint8_t*)decoded.data();

      // Pack the decoded plane into the mode LUT. The destination column
      // index runs 0..iVar27*iVar27-1 across the whole block: for source
      // row iVar26 it starts at iVar26*iVar27 (iVar28) and runs a full
      // iVar27-wide span. This must match load_waveform @0x458e8 exactly,
      // otherwise the display-thread gather reads out-of-range LUT indices.
      int iVar28 = 0;
      int iVar24 = iVar27;
      for (int iVar26 = 0; iVar26 < iVar27; iVar26++) {
        int iVar19 = iVar28;
        uint8_t* iVar29 = src + iVar26;
        do {
          if (uVar14 != 0) {
            uint32_t uVar15 = 0;
            uint32_t uVar16 = 0;
            while (uVar16 < (uint32_t)uVar14) {
              uint32_t uVar9 = uVar16 + 1;
              uint32_t uVar1 = (uint32_t)iVar29[uVar16 * 0x400]
                               << ((uVar16 & 7) * 2);
              uint16_t uVar5 = (uint16_t)uVar15;
              uVar15 = uVar15 | (uVar1 & 0xffff);
              if ((uVar9 & 7) != 0 && (uint32_t)(uVar14 - 1) != uVar16) {
                uVar16 = uVar9;
                continue;
              }
              dest[(uVar16 >> 3) * 0x401 + iVar19] = uVar5 | (uint16_t)uVar1;
              uVar15 = 0;
              uVar16 = uVar9;
            }
          }
          iVar19++;
          iVar29 += iVar27;
        } while (iVar19 != iVar24);
        iVar28 += iVar27;
        iVar24 += iVar27;
      }

      mode->luts.push_back(lut);
    }

    waveform_struct->push_back(mode);
  }

  return true;
}

namespace {

// The real library's search order (qsgepaper_init's directory-list
// construction, DAT_00066de0/de4/de8 in the library's .rodata).
const char* const kWaveformSearchDirs[] = {
  "/var/lib/remarkable/",
  "/var/lib/uboot/",
  "/usr/share/remarkable/",
};

} // namespace

// Not in the anonymous namespace above (unlike read_factory_barcode below):
// these five are pure - no hardcoded device paths of their own - so they're
// declared in init.h and unit-tested directly against synthetic inputs
// (temp dirs, in-memory barcode strings, small fixture files) instead of
// only indirectly through find_waveform_path's real /dev/mmcblk2boot1 +
// kWaveformSearchDirs orchestration, which isn't something a host test can
// drive at all.

bool
has_wbf_suffix(const char* name) {
  size_t len = strlen(name);
  if (len < 4)
    return false;
  const char* suffix = name + len - 4;
  return suffix[0] == '.' && tolower((unsigned char)suffix[1]) == 'w' &&
         tolower((unsigned char)suffix[2]) == 'b' &&
         tolower((unsigned char)suffix[3]) == 'f';
}

// Lists every *.wbf (case-insensitive) regular-file dirent in `dir`,
// appending its full path to *out. A missing directory is silently skipped
// (mirrors get_waveform_path's own opendir-failure handling, which just
// moves on to the next directory in the list).
void
list_wbf_files(const std::string& dir, std::vector<std::string>* out) {
  DIR* d = opendir(dir.c_str());
  if (!d)
    return;
  struct dirent* entry;
  while ((entry = readdir(d)) != nullptr) {
    if (entry->d_type != DT_REG)
      continue;
    if (!has_wbf_suffix(entry->d_name))
      continue;
    out->push_back(dir + entry->d_name);
  }
  closedir(d);
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
  if (sz < 0x20)
    return false;
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(sz);
  if (!file.read((char*)data.data(), sz))
    return false;

  *fpl_lot = *(uint16_t*)(data.data() + 0xe);

  tft_vid[0] = '0';
  tft_vid[1] = '0';
  tft_vid[2] = '\0';
  uint32_t xwia = (*(uint32_t*)(data.data() + 0x1c)) & 0xffffff;
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

// Mirrors FUN_00053528: decodes a two-character FPL_LOT pair (barcode bytes
// [6:8]) into a single int. Alphabet skips 'I'/'O' (a classic lot-code
// convention avoiding confusion with 1/0). Empirically confirmed against a
// real on-device barcode - bytes "AD" decoded to 333, matching that same
// device's own /var/lib/uboot/320_R333_..._TC.wbf (per an actual xochitl
// log line: "Loading waveforms from: /var/lib/uboot/320_R333_..."). Only the
// digit/'A'-'H' first-letter branch has been empirically validated this way;
// the 'J'-'N'/'Q'-'Z' branches below are [derived] from Ghidra disassembly
// alone.
int
decode_fpl_lot_pair(char c1, char c2) {
  int v1 = (uint8_t)c1 - 0x30;
  int iVar1;
  if (v1 >= 0 && v1 <= 9) {
    iVar1 = v1;
  } else if (v1 >= 0x11 && v1 <= 0x18) {
    iVar1 = c1 - 0x37;
  } else if (v1 >= 0x1a && v1 <= 0x1e) {
    iVar1 = c1 - 0x38;
  } else if (v1 >= 0x21 && v1 <= 0x2a) {
    iVar1 = c1 - 0x3a;
  } else {
    return -1;
  }

  int v2 = (uint8_t)c2 - 0x30;
  if (v1 <= 0x18) {
    // First letter was digit/'A'-'H': second can be
    // digit/'A'-'H'/'J'-'N'/'Q'-'Z'.
    if (v2 >= 0 && v2 <= 9) {
      if (iVar1 == 0 && v2 == 0)
        return -1;
      return iVar1 * 10 + v2;
    } else if (v2 >= 0x11 && v2 <= 0x18) {
      return iVar1 * 0x17 + (c2 - 0x37) + 0x5a;
    } else if (v2 >= 0x1a && v2 <= 0x1e) {
      return iVar1 * 0x17 + (c2 - 0x38) + 0x5a;
    } else if (v2 >= 0x21 && v2 <= 0x2a) {
      return iVar1 * 0x17 + (c2 - 0x3a) + 0x5a;
    }
    return -1;
  } else {
    // First letter was 'J'-'N'/'Q'-'Z': second letter must be a digit.
    if ((uint8_t)c2 >= 0x30 && (uint8_t)c2 <= 0x39)
      return iVar1 * 10 + (c2 - 0x30);
    return -1;
  }
}

// Mirrors FUN_000537b4: decodes the factory barcode's TFT_VID (bytes [0:3],
// fixed lookup table) and FPL_LOT (bytes [6:8] via decode_fpl_lot_pair, only
// if byte[5]=='R'). A TFT_VID lookup miss still succeeds (falls back to
// "00", matching the real "Unable to decode TFT_VID from barcode." warning
// path) - only a missing/invalid FPL_LOT fails the whole decode, at which
// point the caller reverts to wildcard-everything matching (matches
// get_waveform_path, which never uses a partially-decoded barcode).
bool
decode_barcode(const std::string& barcode, char tft_vid[3], int* fpl_lot) {
  if (barcode.size() != 25)
    return false;
  const char* b = barcode.data();

  if (b[0] == 'E' && b[1] == 'U' && b[2] == 'F') {
    tft_vid[0] = 'U';
    tft_vid[1] = '2';
  } else if (b[0] == 'E' && b[1] == 'R' && b[2] == 'L') {
    tft_vid[0] = 'M';
    tft_vid[1] = '1';
  } else if (b[0] == 'H' && b[1] == '0' && b[2] == '4') {
    tft_vid[0] = 'U';
    tft_vid[1] = '3';
  } else if (b[0] == 'H' && b[1] == '1' && b[2] == 'V') {
    tft_vid[0] = 'C';
    tft_vid[1] = '5';
  } else if (b[0] == 'H' && b[1] == '3' && b[2] == 'Y') {
    tft_vid[0] = 'C';
    tft_vid[1] = '6';
  } else {
    tft_vid[0] = '0';
    tft_vid[1] = '0';
  }
  tft_vid[2] = '\0';

  if (b[5] != 'R')
    return false;
  int lot = decode_fpl_lot_pair(b[6], b[7]);
  if (lot == -1)
    return false;
  *fpl_lot = lot;
  return true;
}

namespace {

// Mirrors get_waveform_path's own /dev/mmcblk2boot1 read: 4 big-endian
// length-prefixed fields; field index 3 is the 25-byte factory calibration
// barcode. Confirmed against a real boot1 dump: field 3 decoded to
// "EUFZBRAD1V9V00A6TAT -1.39", a well-formed 25-byte barcode whose decode
// (see decode_barcode/decode_fpl_lot_pair) matches that unit's real
// waveform file exactly. Reads a hardcoded real device path - unlike the
// five functions above, there's no synthetic-input seam worth adding here,
// so this one stays internal.
bool
read_factory_barcode(std::string* out_barcode) {
  FILE* f = fopen64("/dev/mmcblk2boot1", "rb");
  if (!f)
    return false;

  std::string field;
  bool ok = false;
  for (int i = 0; i < 4; i++) {
    uint8_t len_be[4];
    if (fread(len_be, 4, 1, f) != 1) {
      ok = false;
      break;
    }
    uint32_t len = (uint32_t(len_be[0]) << 24) | (uint32_t(len_be[1]) << 16) |
                   (uint32_t(len_be[2]) << 8) | uint32_t(len_be[3]);
    if (len >= 0xfe) {
      ok = false;
      break;
    }
    field.resize(len);
    if (len > 0 && fread(&field[0], len, 1, f) != 1) {
      ok = false;
      break;
    }
    ok = true;
  }
  fclose(f);
  if (!ok)
    return false;
  *out_barcode = field;
  return true;
}

} // namespace

bool
find_waveform_path(std::string* out_path) {
  out_path->clear();

  // Fast path: postinst-waveform (run once at OS install/update time, by a
  // separate "csl" tool) leaves exactly one already-matched .wbf in
  // /var/lib/uboot/ on any device that's been through that flow, deleting
  // any others there first - if that holds, skip the barcode dance
  // entirely. Confirmed against a real device: its /var/lib/uboot/ held
  // exactly one file, and a real xochitl log line showed it loading that
  // exact same file.
  std::vector<std::string> uboot_wbfs;
  list_wbf_files("/var/lib/uboot/", &uboot_wbfs);
  if (uboot_wbfs.size() == 1) {
    std::cout << "find_waveform_path: single .wbf in /var/lib/uboot/, using it "
                 "directly: "
              << uboot_wbfs[0] << std::endl;
    *out_path = uboot_wbfs[0];
    return true;
  }

  // Fallback: full barcode-based matching across all three search
  // directories, mirroring get_waveform_path/FUN_00051fd0. Needed for
  // older devices/layouts that never went through the postinst-waveform
  // flow above - confirmed to still exist: an early-2020 rM2 image has
  // several loose, unmatched .wbf files directly under
  // /usr/share/remarkable/ and nothing in /var/lib/uboot/.
  char target_tft_vid[3] = { '0', '0', '\0' };
  int target_fpl_lot = -1;

  std::string barcode;
  if (read_factory_barcode(&barcode)) {
    if (!decode_barcode(barcode, target_tft_vid, &target_fpl_lot)) {
      target_tft_vid[0] = '0';
      target_tft_vid[1] = '0';
      target_tft_vid[2] = '\0';
      target_fpl_lot = -1;
    }
  } else {
    std::cerr
      << "find_waveform_path: unable to read barcode from /dev/mmcblk2boot1"
      << std::endl;
  }

  std::vector<std::string> candidates;
  for (const char* dir : kWaveformSearchDirs) {
    list_wbf_files(dir, &candidates);
  }
  if (candidates.empty()) {
    std::cerr << "find_waveform_path: unable to find any waveform files!"
              << std::endl;
    return false;
  }

  std::string exact_match, fpl_lot_only, tft_vid_only, last_seen;
  for (const auto& path : candidates) {
    uint16_t file_fpl_lot;
    char file_tft_vid[3];
    if (!read_wbf_match_fields(path, &file_fpl_lot, file_tft_vid))
      continue;

    last_seen = path;
    bool fpl_lot_ok =
      target_fpl_lot != -1 && file_fpl_lot == (uint16_t)target_fpl_lot;
    bool tft_vid_ok = strcmp(target_tft_vid, file_tft_vid) == 0;
    if (fpl_lot_ok && tft_vid_ok)
      exact_match = path;
    else if (fpl_lot_ok)
      fpl_lot_only = path;
    else if (tft_vid_ok)
      tft_vid_only = path;
  }

  if (!exact_match.empty()) {
    *out_path = exact_match;
  } else if (!fpl_lot_only.empty()) {
    std::cout << "Waveform file with correct TFT_VID not found, using fallback "
                 "matching only on FPL_LOT: "
              << fpl_lot_only << std::endl;
    *out_path = fpl_lot_only;
  } else if (!tft_vid_only.empty()) {
    std::cout << "Waveform file with correct FPL_LOT not found, using fallback "
                 "matching only on TFT_VID: "
              << tft_vid_only << std::endl;
    *out_path = tft_vid_only;
  } else {
    std::cout << "Waveform file with correct FPL_LOT and TFT_VID not found, "
                 "using fallback: "
              << last_seen << std::endl;
    *out_path = last_seen;
  }
  return true;
}

// Mirrors find_temperature_hwmon_path (0x46924): scans /sys/class/hwmon for
// the entry whose "name" file reads "sy7636a_temperature", then confirms
// .../temp0 exists (mirroring the original's __stat64_time64 check) before
// returning that path. Keeps scanning past entries that don't match or whose
// temp0 is missing; returns false with *out_path cleared if none match.
bool
find_temperature_hwmon_path(std::string* out_path) {
  out_path->clear();

  DIR* dir = opendir("/sys/class/hwmon");
  if (!dir)
    return false;

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    std::string name_path =
      std::string("/sys/class/hwmon/") + entry->d_name + "/name";
    FILE* f = fopen64(name_path.c_str(), "r");
    if (!f) {
      std::cout << "unable to open: " << name_path << std::endl;
      continue;
    }
    char buf[256] = { 0 };
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[strcspn(buf, "\r\n")] = 0;

    if (strcmp(buf, "sy7636a_temperature") != 0)
      continue;

    std::string temp_path =
      std::string("/sys/class/hwmon/") + entry->d_name + "/temp0";
    struct stat st;
    if (stat(temp_path.c_str(), &st) != 0)
      continue;

    *out_path = temp_path;
    closedir(dir);
    return true;
  }
  closedir(dir);
  return false;
}

// Mirrors read_temperature_raw (0x46644): reads a hwmon sysfs value file and
// strtol-parses the leading integer.
bool
read_temperature_raw(const char* path, int* out_value) {
  FILE* f = fopen64(path, "r");
  if (!f) {
    std::cerr << "temperature_hwmon: unable to open temperature file: " << path
              << std::endl;
    return false;
  }
  char buf[256] = { 0 };
  size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  fclose(f);
  if (n == 0) {
    std::cerr << "temperature_hwmon: unable to read temperature file: " << path
              << std::endl;
    return false;
  }
  if (buf[0] == '\0') {
    std::cerr << "temperature_hwmon: buffer is empty" << std::endl;
    return false;
  }

  errno = 0;
  char* endptr = nullptr;
  long value = strtol(buf, &endptr, 10);
  if (endptr == buf) {
    std::cerr << "temperature_hwmon: no digits found" << std::endl;
    return false;
  }
  if (errno == ERANGE) {
    std::cerr << "temperature_hwmon: conversion to a number failed: '" << buf
              << "'" << std::endl;
    return false;
  }

  *out_value = (int)value;
  return true;
}

void
refresh_temperature_cache() {
  int raw_value;
  if (!read_temperature_raw(swtcon_state()->hwmonTempPath.c_str(), &raw_value))
    return;

  std::lock_guard<std::mutex> lock(*temperature_mutex());
  *cached_temperature_ptr() = (float)raw_value - 2.0f;
}

void
init_temperature_sensor() {
  auto* state = swtcon_state();
  if (find_temperature_hwmon_path(&state->hwmonTempPath)) {
    std::cout << "temperature_hwmon: found temperature path: "
              << state->hwmonTempPath << std::endl;
  } else {
    std::cerr << "temperature_hwmon: failed to find path" << std::endl;
  }
  refresh_temperature_cache();
}

void*
frame_buffer_addr(int frame_idx) {
  auto* fb = framebuffer_globals();
  return (uint8_t*)fb->pFbMmap + (size_t)fb->nFbSizeX * frame_idx;
}

void
upload_lut_to_frame_slot(void* dest) {
  memcpy(dest, framebuffer_globals()->pLUT, kLutBlobSize);
}

// Mirrors reset_statebuffer_neutral (0x4fbe0): reapplies the same
// kNeutralStateWord-per-pixel fill init_statebuffer used at
// allocation time, over the already-allocated statebuffer. Called by the
// worker thread after its flash sequence to reset per-pixel state to
// neutral.
void
reset_statebuffer_neutral() {
  auto* sb = statebuffer_globals();
  size_t sz = kStatebufferSize;
  uint32_t* sp = (uint32_t*)sb->pStatebuffer;
  for (size_t i = 0; i < sz / 4; i++)
    sp[i] = kNeutralStateWord;
}

// Mirrors read_lut_packed_pixel (0x40c58): generic bit-unpacking read of one
// packed pixel from a LUTEntry - the read-side inverse of load_waveform's
// packing loop above (which hardcodes bit_depth=2; this generalizes to any
// bit_depth). `phase` selects which of the LUT's packed planes to read from.
unsigned
read_lut_packed_pixel(const LUTEntry* lut, int row, int col, int phase) {
  int pixels_per_word = 16 / lut->bit_depth;
  int word_idx = phase / pixels_per_word;
  int bit_off = phase - pixels_per_word * word_idx;
  int mw = lut->mode_width;
  int data_idx = mw * row + col + mw * mw * word_idx + word_idx;
  uint16_t raw = ((const uint16_t*)lut->data)[data_idx];
  return (raw >> (lut->bit_depth * bit_off)) & ((1u << lut->bit_depth) - 1);
}

int
pan_and_unblank(int frame_idx) {
  auto* fb = framebuffer_globals();
  auto* vinfo = &fb->fbVar;

  if (fb->nIsFbBlanked == 0)
    return 1;

  vinfo->yoffset = frame_idx * vinfo->yres;

  if (ioctl(fb->nFbFd, FBIOPUT_VSCREENINFO, vinfo) == -1) {
    std::cerr << "Panning failed! start buffer = " << frame_idx << ", "
              << strerror(errno) << std::endl;
    return -1;
  }

  for (int retry = 5; retry > 0; retry--) {
    if (ioctl(fb->nFbFd, FBIOBLANK, FB_BLANK_UNBLANK) != -1) {
      fb->nIsFbBlanked = 0;
      return 0;
    }
    std::cout << "FBIOBLANK failed (unblank)" << std::endl;
  }

  vinfo->yoffset = fb->nFbSizeY * vinfo->yres;
  if (ioctl(fb->nFbFd, FBIOPAN_DISPLAY, vinfo) == -1) {
    std::cout << "Pan failed" << std::endl;
  }
  return -1;
}

// Mirrors pan_to_frame (unconditional pan, no unblank/retry - unlike
// pan_and_unblank, which also flips the panel on and retries FBIOBLANK
// up to 5 times): sets fbVar.yoffset and issues FBIOPAN_DISPLAY once, logging
// "Pan failed" on error and otherwise doing nothing else.
void
pan_to_frame(int frame_idx) {
  auto* fb = framebuffer_globals();
  auto* vinfo = &fb->fbVar;
  vinfo->yoffset = frame_idx * vinfo->yres;
  if (ioctl(fb->nFbFd, FBIOPAN_DISPLAY, vinfo) == -1) {
    std::cout << "Pan failed" << std::endl;
  }
}

void
prime_display() {
  if (is_fb_blanked() == 0) {
    refresh_temperature_cache();
    return;
  }
  pan_and_unblank(kInitFrameSlotIndex);
  refresh_temperature_cache();
  blank_fb();
}

int
is_fb_blanked() {
  return framebuffer_globals()->nIsFbBlanked;
}

void
blank_fb() {
  auto* fb = framebuffer_globals();
  if (fb->nIsFbBlanked == 0) {
    fb->nIsFbBlanked = 1;
    if (ioctl(fb->nFbFd, FBIOBLANK, FB_BLANK_POWERDOWN) == -1) {
      std::cerr << "FBIOBLANK failed (blank)" << std::endl;
    }
  }
}

void
save_statebuffer(uintptr_t state_ptr_or_zero) {
  auto* sb = statebuffer_globals();
  char* filename = (char*)state_ptr_or_zero;
  FILE* f = fopen64(filename, "w");
  if (f) {
    fwrite(sb->pStatebuffer, sb->nSize, 1, f);
    fclose(f);
  }
}

void
free_statebuffer() {
  auto* sb = statebuffer_globals();
  free(sb->pStatebuffer);
  free(sb->pGammaTable);
}

void
close_fb() {
  auto* fb = framebuffer_globals();
  munmap(fb->pFbMmap, (size_t)fb->nFbSizeX * fb->nFbSizeY);
  fb->pFbMmap = nullptr;
  close(fb->nFbFd);
  fb->nFbFd = -1;
}

void
free_LUT() {
  auto* fb = framebuffer_globals();
  free(fb->pLUT);
}

void
unlock_pid_file() {
  auto* state = swtcon_state();
  if (state->nPidFd > -1) {
    if (flock(state->nPidFd, LOCK_UN) == -1) {
      std::cerr << "unable to unlock exclusive lock" << std::endl;
    }
    close(state->nPidFd);
  }
}
