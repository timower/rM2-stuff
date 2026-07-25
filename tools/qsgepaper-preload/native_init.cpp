#include "native_init.h"
#include "statebuffer_table.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// The library keeps four separate buffers (see init_statebuffer + the
// qsgepaper_init prologue). They are distinct allocations with distinct fills:
//   g_pImageBufferNative  -> g_pDataBuffer  @0x670bc, 0x503580, memset 0xff
//                            (the 16-bit image buffer returned to the caller)
//   g_pScreenBufferNative -> g_pBackBuffer  @0x670c0, 0x281ac0, calloc
//                            (full-screen 1 byte/pixel back buffer)
//   g_pStateBufferNative  -> DAT_0006d1d0   @0x6d1d0, 0x503580, memset 0x1e
//                            (the persisted statebuffer)
//   g_pGammaTableNative   -> DAT_0006d1d4   @0x6d1d4, 0x4400 ('U' + gamma LUT,
//                            128 temperature entries x 0x88 bytes) read by the
//                            render kernel FUN_0004e7b8 as uVar40*0x88 + base.
void* g_pImageBufferNative = nullptr;
void* g_pScreenBufferNative = nullptr;
void* g_pStateBufferNative = nullptr;
void* g_pGammaTableNative = nullptr;

int g_nPidFdNative = -1;
int g_nFbFdNative = -1;
int g_nFbSizeXNative = 0;
int g_nFbSizeYNative = 0;
void* g_pFbAddrNative = nullptr;
void* g_pLUTAddrNative = nullptr;

// The library's pan/display code reads the *global* g_fbVarScreeninfo (0x6d3a0)
// and g_fbFixScreeninfo (0x6d35c) — pan_to_frame just rewrites yoffset in that
// global and re-issues FBIOPAN_DISPLAY. So native_init_framebuffer must fill
// these (not locals); swtcon_init copies them into the library globals. If the
// global vinfo is left zeroed the pan ioctl gets yres=0/xres=0 and the driver
// rejects it ("Pan failed"), which is why the panel never refreshes on hardware.
struct fb_var_screeninfo g_fbVarScreeninfoNative;
struct fb_fix_screeninfo g_fbFixScreeninfoNative;

LUTEntry::~LUTEntry() {
    if (data) free(data); // Using free() since malloc() was used
}

int native_create_pid_file() {
    int fd = open("/tmp/epd.lock", O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        std::cerr << "unable to open lock file: /tmp/epd.lock" << std::endl;
        return -1;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        g_nPidFdNative = fd;
        return 0;
    }
    std::cerr << "another instance is already running" << std::endl;
    close(fd);
    return -1;
}

int native_init_statebuffer() {
    size_t sz = 0x503580;

    // g_pDataBuffer: 16-bit image working buffer, returned to the caller.
    g_pImageBufferNative = malloc(sz);
    if (!g_pImageBufferNative) return -1;
    memset(g_pImageBufferNative, 0xff, sz);

    // g_pBackBuffer: full-screen 1 byte/pixel back buffer (1404*1872 = 0x281ac0).
    g_pScreenBufferNative = calloc(0x281ac0, 1);
    if (!g_pScreenBufferNative) return -1;

    // DAT_0006d1d0: the persisted statebuffer. init_statebuffer @0x4fad4 fills it
    // with the 32-bit pattern 0x001e001e (bytes 1e 00 1e 00) — i.e. a per-pixel
    // uint16 state of 0x001e, NOT every byte = 0x1e. Using a plain memset(0x1e)
    // makes each state 0x1e1e and corrupts the waveform transitions the render
    // kernels compute.
    g_pStateBufferNative = malloc(sz);
    if (!g_pStateBufferNative) return -1;
    {
        uint32_t* sp = (uint32_t*)g_pStateBufferNative;
        for (size_t i = 0; i < sz / 4; i++)
            sp[i] = 0x001e001e;
    }

    // DAT_0006d1d4: 'U' followed by the gamma LUT (128 entries x 0x88 bytes).
    g_pGammaTableNative = malloc(0x4400);
    if (!g_pGammaTableNative) return -1;
    // Matches init_statebuffer @0x4fad4: values are treated as UNSIGNED 16-bit
    // (the library does VectorSignedToFloat((uint)*puVar6) on a zero-extended
    // ushort). statebuffer_table is pre-shifted to start at the library's first
    // read (Ghidra 0x596da), so index i maps directly.
    char* pc = (char*)g_pGammaTableNative;
    *pc++ = 'U';
    for (int i = 0; i < 17407; i++) {
        double d = (double)statebuffer_table[i];
        d = round((d * 124.0) / 65532.0);
        *pc++ = (d > 0.0) ? (char)d : 0;
    }

    return 0;
}

int native_init_framebuffer(int* fb_info) {
    const char* file = "/dev/fb0";
    g_nFbFdNative = open(file, O_RDWR);
    if (g_nFbFdNative < 0) {
        std::cerr << "Cannot open device" << std::endl;
        return -1;
    }

    // Fill the module-global finfo/vinfo (not locals): the display/pan code reads
    // g_fbVarScreeninfo directly, so the full struct must persist past init.
    struct fb_fix_screeninfo& finfo = g_fbFixScreeninfoNative;
    struct fb_var_screeninfo& vinfo = g_fbVarScreeninfoNative;
    memset(&finfo, 0, sizeof(finfo));
    if (ioctl(g_nFbFdNative, FBIOGET_FSCREENINFO, &finfo) == -1) {
        std::cerr << "Error reading fixed information" << std::endl;
        return -1;
    }

    // Read the current mode, then overwrite exactly the fields init_framebuffer
    // (@0x53c0c) sets, leaving everything else as the driver reported it.
    memset(&vinfo, 0, sizeof(vinfo));
    if (ioctl(g_nFbFdNative, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        std::cerr << "Unable to read screeninfo" << std::endl;
        return -1;
    }

    vinfo.xres = fb_info[1];
    vinfo.yres = fb_info[2];
    vinfo.xres_virtual = fb_info[1];
    vinfo.yres_virtual = fb_info[2] * (fb_info[11] + 1);
    vinfo.yoffset = fb_info[11] * fb_info[2];
    vinfo.bits_per_pixel = fb_info[3];
    vinfo.pixclock = fb_info[4];
    vinfo.left_margin = fb_info[5];
    vinfo.right_margin = fb_info[6];
    vinfo.upper_margin = fb_info[7];
    vinfo.lower_margin = fb_info[8];
    vinfo.hsync_len = fb_info[9];
    vinfo.vsync_len = fb_info[10];

    if (ioctl(g_nFbFdNative, FBIOPUT_VSCREENINFO, &vinfo) == -1) {
        std::cerr << "Error writing variable information" << std::endl;
        return -1;
    }

    // g_nFbSizeX is one full frame in bytes (bpp*xres*yres/8); g_nFbSizeY is the
    // number of stacked frames (fb_info[11]+1). The mmap covers all of them.
    int y_factor = (char)(fb_info[11] + 1);
    unsigned int frame_bytes =
      (unsigned int)(fb_info[3] * fb_info[1] * fb_info[2]) >> 3;
    size_t size = (size_t)y_factor * frame_bytes;

    g_pFbAddrNative =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, g_nFbFdNative, 0);
    if (g_pFbAddrNative == MAP_FAILED) {
        std::cerr << "Error: failed to map framebuffer device to memory" << std::endl;
        return -1;
    }

    g_nFbSizeXNative = frame_bytes;
    g_nFbSizeYNative = y_factor;

    return 0;
}

// Mirrors FUN_00053a30 exactly (constants read from its disassembly — the vector
// decompilation of the immediates is misleading). vmov/vorr .i32 immediates are
// byte-position encoded: #0x410000 == 0x00410000, #0x20000 == 0x00020000, etc.
void native_init_lut_sub(uint32_t* param_1, int param_2) {
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

    if (param_2 == -1) return;

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

int native_init_lut() {
    size_t sz = 0x165800;
    g_pLUTAddrNative = malloc(sz);
    if (!g_pLUTAddrNative) return -1;
    
    uint32_t* buf = (uint32_t*)g_pLUTAddrNative;

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
    // real param (0).
    native_init_lut_sub(buf + 0x104, -1);
    native_init_lut_sub(buf + 0x208, -1);
    native_init_lut_sub(buf + 0x30c, 0);
    
    uint32_t* puVar2 = buf + 0x410;
    while (puVar2 != buf + 0x59600) {
        memcpy(puVar2, buf + 0x30c, 0x410); // Wait, 0x410 bytes = 0x104 words
        puVar2 += 0x104;
    }
    
    return 0;
}

bool native_load_waveform(std::vector<ModeEntry*>* waveform_struct, const char* path) {
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
        std::cerr << "File length mismatch: " << sz << " != " << hdr_sz << " (hdr)" << std::endl;
        return false;
    }
    
    uint8_t* ptr = wbf.data();
    uint8_t mode_count = ptr[0x25];
    uint8_t temp_count = ptr[0x26];
    uint8_t pad_val = ptr[0x28];
    uint32_t mode_table_offset = *(uint32_t*)(ptr + 0x20) & 0xffffff;
    
    int iVar27 = ((ptr[0x24] & 0xc) == 4) ? 0x20 : 0x10;
    
    const char* mode_names[] = {
        "INIT", "DU", "GC16", "GL16", "GC16_REGAL", "GL16_REGAL",
        "A2", "DU4", "mode8", "mode9", "mode10", "mode11"
    };
    
    // mode_count (ptr[0x25]) is an inclusive maximum index, matching load_waveform
    // @0x458e8 which loops modes 0..ptr[0x25].
    for (int mode_idx = 0; mode_idx <= mode_count; mode_idx++) {
        ModeEntry* mode = new ModeEntry();
        if (mode_idx < 12) mode->name = mode_names[mode_idx];
        else mode->name = "mode" + std::to_string(mode_idx);
        
        for (int temp_idx = 0; temp_idx <= temp_count; temp_idx++) {
            uint32_t mode_table = mode_table_offset;
            uint32_t temp_table_offset = *(uint32_t*)(ptr + mode_table + mode_idx * 4) & 0xffffff;
            uint32_t wave_offset = *(uint32_t*)(ptr + temp_table_offset + temp_idx * 4) & 0xffffff;
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
                    if (bVar1) { i++; run = wave_data[i] + 1; }
                    else { run = 1; }
                    out_size += run * 4;
                }
                i++;
                curr = wave_data[i];
            } while (curr != pad_val);

            if (out_size == 0) continue;

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
                    if (bVar1) { i++; run = wave_data[i] + 1; }
                    else { run = 1; }
                    uint32_t val = (curr & 3) | (((curr & 0xf) >> 2) << 8) | (((curr & 0x3f) >> 4) << 16) | ((curr >> 6) << 24);
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
                            uint32_t uVar1 = (uint32_t)iVar29[uVar16 * 0x400] << ((uVar16 & 7) * 2);
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

// Mirrors find_temperature_hwmon_path (0x46924): scans /sys/class/hwmon for
// the entry whose "name" file reads "sy7636a_temperature", then confirms
// .../temp0 exists (mirroring the original's __stat64_time64 check) before
// returning that path. Keeps scanning past entries that don't match or whose
// temp0 is missing; returns false with *out_path cleared if none match.
bool native_find_temperature_hwmon_path(std::string* out_path) {
    out_path->clear();

    DIR* dir = opendir("/sys/class/hwmon");
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        std::string name_path = std::string("/sys/class/hwmon/") + entry->d_name + "/name";
        FILE* f = fopen64(name_path.c_str(), "r");
        if (!f) {
            std::cout << "unable to open: " << name_path << std::endl;
            continue;
        }
        char buf[256] = {0};
        fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[strcspn(buf, "\r\n")] = 0;

        if (strcmp(buf, "sy7636a_temperature") != 0)
            continue;

        std::string temp_path = std::string("/sys/class/hwmon/") + entry->d_name + "/temp0";
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
bool native_read_temperature_raw(const char* path, int* out_value) {
    FILE* f = fopen64(path, "r");
    if (!f) {
        std::cerr << "temperature_hwmon: unable to open temperature file: " << path << std::endl;
        return false;
    }
    char buf[256] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) {
        std::cerr << "temperature_hwmon: unable to read temperature file: " << path << std::endl;
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
        std::cerr << "temperature_hwmon: conversion to a number failed: '" << buf << "'" << std::endl;
        return false;
    }

    *out_value = (int)value;
    return true;
}
