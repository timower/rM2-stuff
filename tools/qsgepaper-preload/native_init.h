#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

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

// Re-implemented functions
int native_create_pid_file();
int native_init_statebuffer();
int native_init_framebuffer(int* fb_info);
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
