#include "init.h"
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
//   g_pImageBufferNative  -> g_pDataBuffer  @0x670bc, kStatebufferSize, memset 0xff
//                            (the 16-bit image buffer returned to the caller)
//   g_pScreenBufferNative -> g_pBackBuffer  @0x670c0, kScreenWidth*kScreenHeight,
//                            calloc (full-screen 1 byte/pixel back buffer)
//   g_pStateBufferNative  -> DAT_0006d1d0   @0x6d1d0, kStatebufferSize, memset 0x1e
//                            (the persisted statebuffer)
//   g_pGammaTableNative   -> DAT_0006d1d4   @0x6d1d4, kGammaTableSize ('U' + gamma
//                            LUT, 128 temperature entries x 0x88 bytes) read by the
//                            render kernel FUN_0004e7b8 as uVar40*0x88 + base.
void* g_pImageBufferNative = nullptr;
void* g_pScreenBufferNative = nullptr;
void* g_pStateBufferNative = nullptr;
void* g_pGammaTableNative = nullptr;

// Whether g_pImageBufferNative/g_pScreenBufferNative were malloc'd/calloc'd
// here (and so need freeing at shutdown) versus supplied externally by the
// caller (e.g. rm2fb's server passing in its own mmap'd shared framebuffer
// - see swtcon_init's dataBuffer/backBuffer params). free()ing an
// externally-supplied buffer would hand a non-heap pointer to the
// allocator - confirmed on real hardware as an immediate "free(): invalid
// pointer" abort in swtcon_shutdown.
bool g_imageBufferOwnedNative = false;
bool g_screenBufferOwnedNative = false;

int g_nPidFdNative = -1;
int g_nFbFdNative = -1;
int g_nFbSizeXNative = 0;
int g_nFbSizeYNative = 0;
void* g_pFbAddrNative = nullptr;
void* g_pLUTAddrNative = nullptr;

// The library's pan/display code reads the *global* g_fbVarScreeninfo (0x6d3a0)
// and g_fbFixScreeninfo (0x6d35c) — pan_to_frame just rewrites yoffset in that
// global and re-issues FBIOPAN_DISPLAY. So init_framebuffer must fill
// these (not locals); swtcon_init copies them into the library globals. If the
// global vinfo is left zeroed the pan ioctl gets yres=0/xres=0 and the driver
// rejects it ("Pan failed"), which is why the panel never refreshes on hardware.
struct fb_var_screeninfo g_fbVarScreeninfoNative;
struct fb_fix_screeninfo g_fbFixScreeninfoNative;

LUTEntry::~LUTEntry() {
    if (data) free(data); // Using free() since malloc() was used
}

int create_pid_file() {
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

int init_statebuffer(void* dataBuffer, void* backBuffer) {
    size_t sz = kStatebufferSize;

    // g_pDataBuffer: 16-bit image working buffer, returned to the caller.
    g_imageBufferOwnedNative = dataBuffer == nullptr;
    g_pImageBufferNative = dataBuffer == nullptr ? malloc(sz) : dataBuffer;
    if (!g_pImageBufferNative) return -1;
    memset(g_pImageBufferNative, 0xff, sz);

    // g_pBackBuffer: full-screen 1 byte/pixel back buffer.
    g_screenBufferOwnedNative = backBuffer == nullptr;
    g_pScreenBufferNative = backBuffer == nullptr
                               ? calloc((size_t)kScreenWidth * kScreenHeight, 1)
                               : backBuffer;
    if (!g_pScreenBufferNative) return -1;

    // DAT_0006d1d0: the persisted statebuffer. init_statebuffer @0x4fad4 fills it
    // with the 32-bit pattern kNeutralStateWord (bytes 1e 00 1e 00) — i.e. a
    // per-pixel uint16 state of 0x001e, NOT every byte = 0x1e. Using a plain
    // memset(0x1e) makes each state 0x1e1e and corrupts the waveform
    // transitions the render kernels compute.
    g_pStateBufferNative = malloc(sz);
    if (!g_pStateBufferNative) return -1;
    {
        uint32_t* sp = (uint32_t*)g_pStateBufferNative;
        for (size_t i = 0; i < sz / 4; i++)
            sp[i] = kNeutralStateWord;
    }

    // DAT_0006d1d4: 'U' followed by the gamma LUT (128 entries x 0x88 bytes).
    g_pGammaTableNative = malloc(kGammaTableSize);
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

int init_framebuffer(const FbInitParams& fb_info) {
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

    if (ioctl(g_nFbFdNative, FBIOPUT_VSCREENINFO, &vinfo) == -1) {
        std::cerr << "Error writing variable information" << std::endl;
        return -1;
    }

    // g_nFbSizeX is one full frame in bytes (bpp*xres*yres/8); g_nFbSizeY is the
    // number of stacked frames (frameCount+1). The mmap covers all of them.
    int y_factor = (char)(fb_info.frameCount + 1);
    unsigned int frame_bytes =
      (unsigned int)(fb_info.bitsPerPixel * fb_info.xres * fb_info.yres) >> 3;
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
void init_lut_sub(uint32_t* param_1, int param_2) {
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

// Mirrors FUN_00053ac4 exactly: fills a 0x165800-byte LUT-shaped blob (one
// full frame slot's worth) with the fixed dither pattern, parameterized only
// by the third init_lut_sub call's pattern argument - `init_lut`
// passes 0 there (the real waveform LUT); the worker thread's flash sequence
// (write_flash_prime_pattern @0x53c04, wrapping this same function) passes a
// dynamic 16-bit pattern instead. Shared so both callers can't drift apart.
static void fill_lut_pattern(void* dest, int pattern) {
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

int init_lut() {
    size_t sz = kLutBlobSize;
    g_pLUTAddrNative = malloc(sz);
    if (!g_pLUTAddrNative) return -1;
    fill_lut_pattern(g_pLUTAddrNative, 0);
    return 0;
}

// Mirrors write_flash_prime_pattern (0x53c04, wraps FUN_00053ac4): writes the
// same dither-fill algorithm as init_lut, but into an already-existing
// frame-slot-sized buffer and with a caller-supplied pattern rather than the
// real waveform LUT's fixed 0. Used by the worker thread's flash sequence to
// prime frame slots 0-2 with a checkerboard pattern.
void write_lut_pattern(void* dest, int pattern) {
    fill_lut_pattern(dest, pattern);
}

bool load_waveform(std::vector<ModeEntry*>* waveform_struct, const char* path) {
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
bool find_temperature_hwmon_path(std::string* out_path) {
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
bool read_temperature_raw(const char* path, int* out_value) {
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

// Path discovered once by init_temperature_sensor and reused by every
// subsequent refresh_temperature_cache call.
static std::string g_hwmonTempPathNative;

void refresh_temperature_cache() {
    int raw_value;
    if (!read_temperature_raw(g_hwmonTempPathNative.c_str(), &raw_value))
        return;

    pthread_mutex_t* mutex = temperature_mutex();
    float* cached_temp = cached_temperature_ptr();
    pthread_mutex_lock(mutex);
    *cached_temp = (float)raw_value - 2.0f;
    pthread_mutex_unlock(mutex);
}

void init_temperature_sensor() {
    if (find_temperature_hwmon_path(&g_hwmonTempPathNative)) {
        std::cout << "temperature_hwmon: found temperature path: "
                  << g_hwmonTempPathNative << std::endl;
    } else {
        std::cerr << "temperature_hwmon: failed to find path" << std::endl;
    }
    refresh_temperature_cache();
}

void* frame_buffer_addr(int frame_idx) {
    return (uint8_t*)g_pFbAddrNative + (size_t)g_nFbSizeXNative * frame_idx;
}

void upload_lut_to_frame_slot(void* dest) {
    memcpy(dest, g_pLUTAddrNative, kLutBlobSize);
}

// Mirrors reset_statebuffer_neutral (0x4fbe0): reapplies the same
// kNeutralStateWord-per-pixel fill init_statebuffer used at
// allocation time, over the already-allocated statebuffer. Called by the
// worker thread after its flash sequence to reset per-pixel state to
// neutral.
void reset_statebuffer_neutral() {
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
unsigned read_lut_packed_pixel(const LUTEntry* lut, int row, int col, int phase) {
    int pixels_per_word = 16 / lut->bit_depth;
    int word_idx = phase / pixels_per_word;
    int bit_off = phase - pixels_per_word * word_idx;
    int mw = lut->mode_width;
    int data_idx = mw * row + col + mw * mw * word_idx + word_idx;
    uint16_t raw = ((const uint16_t*)lut->data)[data_idx];
    return (raw >> (lut->bit_depth * bit_off)) & ((1u << lut->bit_depth) - 1);
}

int pan_and_unblank(int frame_idx) {
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

    vinfo->yoffset = g_nFbSizeYNative * vinfo->yres;
    if (ioctl(fb->nFbFd, FBIOPAN_DISPLAY, vinfo) == -1) {
        std::cout << "Pan failed" << std::endl;
    }
    return -1;
}

// Mirrors pan_to_frame (unconditional pan, no unblank/retry - unlike
// pan_and_unblank, which also flips the panel on and retries FBIOBLANK
// up to 5 times): sets fbVar.yoffset and issues FBIOPAN_DISPLAY once, logging
// "Pan failed" on error and otherwise doing nothing else.
void pan_to_frame(int frame_idx) {
    auto* fb = framebuffer_globals();
    auto* vinfo = &fb->fbVar;
    vinfo->yoffset = frame_idx * vinfo->yres;
    if (ioctl(fb->nFbFd, FBIOPAN_DISPLAY, vinfo) == -1) {
        std::cout << "Pan failed" << std::endl;
    }
}

void prime_display() {
    if (is_fb_blanked() == 0) {
        refresh_temperature_cache();
        return;
    }
    pan_and_unblank(kInitFrameSlotIndex);
    refresh_temperature_cache();
    blank_fb();
}

int is_fb_blanked() {
    return framebuffer_globals()->nIsFbBlanked;
}

void blank_fb() {
    auto* fb = framebuffer_globals();
    if (fb->nIsFbBlanked == 0) {
        fb->nIsFbBlanked = 1;
        if (ioctl(fb->nFbFd, FBIOBLANK, FB_BLANK_POWERDOWN) == -1) {
            std::cerr << "FBIOBLANK failed (blank)" << std::endl;
        }
    }
}

void save_statebuffer(int state_ptr_or_zero) {
    auto* sb = statebuffer_globals();
    char* filename = (char*)state_ptr_or_zero;
    FILE* f = fopen64(filename, "w");
    if (f) {
        fwrite(sb->pStatebuffer, sb->nSize, 1, f);
        fclose(f);
    }
}

void free_statebuffer() {
    auto* sb = statebuffer_globals();
    free(sb->pStatebuffer);
    if (sb->pGammaTable != nullptr) {
        ::operator delete(sb->pGammaTable);
    }
}

void close_fb() {
    auto* fb = framebuffer_globals();
    munmap(fb->pFbMmap, (size_t)fb->nFbSizeX * fb->nFbSizeY);
    fb->pFbMmap = nullptr;
    close(fb->nFbFd);
    fb->nFbFd = -1;
}

void free_LUT() {
    auto* fb = framebuffer_globals();
    if (fb->pLUT != nullptr) {
        ::operator delete(fb->pLUT);
    }
}

void unlock_pid_file() {
    if (g_nPidFdNative > -1) {
        if (flock(g_nPidFdNative, LOCK_UN) == -1) {
            std::cerr << "unable to unlock exclusive lock" << std::endl;
        }
        close(g_nPidFdNative);
    }
}
