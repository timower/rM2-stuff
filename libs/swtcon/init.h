#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "qsgepaper_globals.h"

struct LUTEntry {
    int size_kb;
    int mode_width;
    float temperature;
    int bit_depth;
    void* data;

    ~LUTEntry();
};

// Sizes of the fixed, single-instance buffers init_statebuffer/
// init_lut allocate - each is duplicated (as a raw literal) at every
// site that (re)allocates, fills, or checksums that buffer, so this is the
// one place all of them should read from. `kStatebufferSize` doubles as the
// 16-bit image buffer's size (UpdateQueueGlobals::dataBuffer) since both are
// kScreenWidth*kScreenHeight 16-bit-per-pixel buffers - see
// init_statebuffer's own comment for which is which.
constexpr size_t kStatebufferSize = (size_t)kScreenWidth * kScreenHeight * 2;
constexpr size_t kGammaTableSize = 0x4400;    // == 128 temperature entries * 0x88 bytes
constexpr size_t kLutBlobSize = 0x165800;     // one full frame slot's worth (bitsPerPixel*xres*yres/8, see FbInitParams in swtcon.cpp)

// Size of the whole panel framebuffer (all kFrameSlotRingCount+1 frame
// slots) - what swtcon_init's real /dev/fb0 mmap covers.
constexpr size_t kFramebufferMemorySize = (size_t)(kFrameSlotRingCount + 1) * kLutBlobSize;

// The neutral per-pixel statebuffer fill (init_statebuffer,
// reset_statebuffer_neutral): each 32-bit word packs two identical
// 16-bit "state = 0x001e" pixels, NOT a byte-wise 0x1e fill (memset(0x1e)
// would produce per-pixel state 0x1e1e instead and corrupt every waveform
// transition the render kernels compute - a real bug this codebase hit
// once, see init_statebuffer's comment).
constexpr uint32_t kNeutralStateWord = 0x001e001e;

struct ModeEntry {
    std::string name;
    std::vector<std::shared_ptr<LUTEntry>> luts;
};

// Parameters for init_framebuffer, replacing the library's raw
// `int fb_info[14]` parameter block (init_framebuffer @0x53c0c). Only 11 of
// the 14 slots are known to be read; the rest are reserved/unused.
struct FbInitParams {
  int32_t _unused0;
  int32_t xres;
  int32_t yres;
  int32_t bitsPerPixel;
  int32_t pixclock;
  int32_t leftMargin;
  int32_t rightMargin;
  int32_t upperMargin;
  int32_t lowerMargin;
  int32_t hsyncLen;
  int32_t vsyncLen;
  int32_t frameCount; // yres_virtual = yres * (frameCount + 1)
  int32_t _unused12;
  int32_t _unused13;
};
static_assert(sizeof(FbInitParams) == 14 * sizeof(int32_t), "FbInitParams must match the library's 14-int parameter block");

// Re-implemented functions
int create_pid_file();
// dataBuffer/backBuffer let a caller supply pre-allocated storage for the
// image working buffer and the full-screen back buffer (e.g. rm2fb's shared
// framebuffer) instead of having init_statebuffer malloc/calloc its own -
// null (the default) keeps the original self-allocating behavior.
int init_statebuffer(void* dataBuffer = nullptr, void* backBuffer = nullptr);
int init_framebuffer(const FbInitParams& fb_info);
// Like init_framebuffer, but mmaps a caller-supplied fd (e.g. memfd_create())
// instead of opening /dev/fb0 - see InitParams::framebufferFd in swtcon.h.
void init_framebuffer_with_fd(const FbInitParams& fb_info, int fd);
int init_lut();
bool load_waveform(std::vector<ModeEntry*>* waveform_struct, const char* path);

// Pure pieces of find_waveform_path's matching algorithm, exposed here for
// direct unit testing - see their definitions in init.cpp.
bool has_wbf_suffix(const char* name);
void list_wbf_files(const std::string& dir, std::vector<std::string>* out);
bool read_wbf_match_fields(const std::string& path, uint16_t* fpl_lot, char tft_vid[3]);
int decode_fpl_lot_pair(char c1, char c2);
bool decode_barcode(const std::string& barcode, char tft_vid[3], int* fpl_lot);

// Mirrors qsgepaper_init's own waveform-path selection (get_waveform_path
// @0x52fb0 + FUN_00051fd0's directory scan): picks which .wbf file to hand
// load_waveform. Fast path first - /var/lib/uboot/ is where the real
// on-device postinst-waveform script (run once at OS install/update time by
// a separate "csl" tool) copies the single already-barcode-matched .wbf for
// this unit, deleting any others - so if exactly one .wbf is sitting there,
// it's already the answer and no barcode parsing is needed. Falls back to
// the full reversed algorithm (read the factory calibration barcode from
// /dev/mmcblk2boot1, decode FPL_LOT/TFT_VID, scan all three of the real
// library's search directories and pick the best-matching .wbf) for older
// devices/layouts that never went through that install flow (confirmed to
// still exist - see the loose, unmatched .wbf files found directly under
// /usr/share/remarkable/ on an early-2020 rM2 image). Returns false (with
// *out_path cleared) only if no .wbf can be found anywhere.
bool find_waveform_path(std::string* out_path);

// Mirrors find_temperature_hwmon_path (0x46924): scans /sys/class/hwmon/*/name
// for the "sy7636a_temperature" sensor and returns its .../temp0 path. Returns
// false (and clears *out_path) if no matching hwmon device is found.
bool find_temperature_hwmon_path(std::string* out_path);

// Mirrors read_temperature_raw (0x46644): reads and strtol-parses the leading
// integer from a hwmon sysfs value file. Returns false if the file can't be
// opened/read or contains no digits.
bool read_temperature_raw(const char* path, int* out_value);

// Mirrors init_temperature_sensor (0x476dc): discovers the sy7636a_temperature
// hwmon sysfs path once, then performs an initial cache refresh so
// get_current_temperature has a valid reading before the display/worker
// threads start.
void init_temperature_sensor();

// Mirrors refresh_temperature_cache (0x4681c): reads the hwmon sensor at the
// discovered path, subtracts the fixed 2.0C calibration offset, and stores
// the result into the library's cached-temperature global under its mutex.
void refresh_temperature_cache();

// Mirrors frame_buffer_addr (0x53fd0): address of frame slot `frame_idx`
// within the mmap'd framebuffer (each slot is framebuffer_globals()->nFbSizeX
// bytes).
void* frame_buffer_addr(int frame_idx);

// Mirrors upload_lut_to_frame_slot (0x53bc8): copies the full waveform LUT
// table into one hardware-visible frame slot.
void upload_lut_to_frame_slot(void* dest);

// Mirrors write_flash_prime_pattern (0x53c04): same dither-fill algorithm as
// init_lut, but into an arbitrary (already-allocated, frame-slot-sized)
// buffer with a caller-supplied 16-bit pattern instead of the real waveform
// LUT's fixed 0.
void write_lut_pattern(void* dest, int pattern);

// Mirrors reset_statebuffer_neutral (0x4fbe0): reapplies the neutral
// 0x001e001e per-pixel fill over the already-allocated statebuffer.
void reset_statebuffer_neutral();

// Mirrors read_lut_packed_pixel (0x40c58): unpacks one bit_depth-wide pixel
// value from a LUTEntry's packed data at (row, col, phase).
unsigned read_lut_packed_pixel(const LUTEntry* lut, int row, int col, int phase);

// Mirrors pan_and_unblank (0x53ebc): pans to `frame_idx` and retries an
// FBIOBLANK unblank up to 5 times.
int pan_and_unblank(int frame_idx);

// Mirrors pan_to_frame (unconditional pan, no unblank/retry - see
// pan_and_unblank for the version worker_thread_func uses when the
// panel might still be blanked).
void pan_to_frame(int frame_idx);

// Mirrors prime_display (0x468f0). Called once from swtcon_init right after
// g_nIsFbBlanked is forced to 1: briefly unblanks frame slot 16 so the panel
// controller has a primed frame, refreshes the temperature cache, then
// reblanks. Without this the frame counters are never seeded and the display
// thread never produces a first visible refresh.
void prime_display();

int is_fb_blanked();
void blank_fb();

// Takes the destination path as a raw pointer-sized int rather than a typed
// pointer, mirroring the real 32-bit ARM ABI (see WorkItem::pixelDataPtr's
// comment in update.h for the same reasoning) - uintptr_t rather than
// int32_t so the round-trip stays lossless on a 64-bit host too.
void save_statebuffer(uintptr_t state_ptr_or_zero);
void free_statebuffer();
void close_fb();
void free_LUT();
void unlock_pid_file();
