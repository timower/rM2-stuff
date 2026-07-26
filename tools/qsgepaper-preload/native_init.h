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

struct ModeEntry {
    std::string name;
    std::vector<std::shared_ptr<LUTEntry>> luts;
};

// Parameters for native_init_framebuffer, replacing the library's raw
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
int native_create_pid_file();
int native_init_statebuffer();
int native_init_framebuffer(const FbInitParams& fb_info);
int native_init_lut();
bool native_load_waveform(std::vector<ModeEntry*>* waveform_struct, const char* path);

// Mirrors find_temperature_hwmon_path (0x46924): scans /sys/class/hwmon/*/name
// for the "sy7636a_temperature" sensor and returns its .../temp0 path. Returns
// false (and clears *out_path) if no matching hwmon device is found.
bool native_find_temperature_hwmon_path(std::string* out_path);

// Mirrors read_temperature_raw (0x46644): reads and strtol-parses the leading
// integer from a hwmon sysfs value file. Returns false if the file can't be
// opened/read or contains no digits.
bool native_read_temperature_raw(const char* path, int* out_value);

// Mirrors init_temperature_sensor (0x476dc): discovers the sy7636a_temperature
// hwmon sysfs path once, then performs an initial cache refresh so
// get_current_temperature has a valid reading before the display/worker
// threads start.
void native_init_temperature_sensor();

// Mirrors refresh_temperature_cache (0x4681c): reads the hwmon sensor at the
// discovered path, subtracts the fixed 2.0C calibration offset, and stores
// the result into the library's cached-temperature global under its mutex.
void native_refresh_temperature_cache();

// Mirrors frame_buffer_addr (0x53fd0): address of frame slot `frame_idx`
// within the mmap'd framebuffer (each slot is g_nFbSizeXNative bytes).
void* native_frame_buffer_addr(int frame_idx);

// Mirrors upload_lut_to_frame_slot (0x53bc8): copies the full waveform LUT
// table into one hardware-visible frame slot.
void native_upload_lut_to_frame_slot(void* dest);

// Mirrors write_flash_prime_pattern (0x53c04): same dither-fill algorithm as
// native_init_lut, but into an arbitrary (already-allocated, frame-slot-sized)
// buffer with a caller-supplied 16-bit pattern instead of the real waveform
// LUT's fixed 0.
void native_write_lut_pattern(void* dest, int pattern);

// Mirrors reset_statebuffer_neutral (0x4fbe0): reapplies the neutral
// 0x001e001e per-pixel fill over the already-allocated statebuffer.
void native_reset_statebuffer_neutral();

// Mirrors read_lut_packed_pixel (0x40c58): unpacks one bit_depth-wide pixel
// value from a LUTEntry's packed data at (row, col, phase).
unsigned native_read_lut_packed_pixel(const LUTEntry* lut, int row, int col, int phase);

// Mirrors pan_and_unblank (0x53ebc): pans to `frame_idx` and retries an
// FBIOBLANK unblank up to 5 times.
int native_pan_and_unblank(int frame_idx);

// Mirrors pan_to_frame (unconditional pan, no unblank/retry - see
// native_pan_and_unblank for the version worker_thread_func uses when the
// panel might still be blanked).
void native_pan_to_frame(int frame_idx);

// Mirrors prime_display (0x468f0). Called once from swtcon_init right after
// g_nIsFbBlanked is forced to 1: briefly unblanks frame slot 16 so the panel
// controller has a primed frame, refreshes the temperature cache, then
// reblanks. Without this the frame counters are never seeded and the display
// thread never produces a first visible refresh.
void native_prime_display();

int native_is_fb_blanked();
void native_blank_fb();

void native_save_statebuffer(int state_ptr_or_zero);
void native_free_statebuffer();
void native_close_fb();
void native_free_LUT();
void native_unlock_pid_file();
