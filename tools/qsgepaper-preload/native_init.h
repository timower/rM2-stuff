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
