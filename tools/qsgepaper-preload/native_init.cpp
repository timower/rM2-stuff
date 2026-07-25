#include "native_init.h"
#include "statebuffer_table.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

void* g_pDataBufferNative = nullptr;
void* g_pBackBufferNative = nullptr;

int g_nPidFdNative = -1;
int g_nFbFdNative = -1;
int g_nFbSizeXNative = 0;
int g_nFbSizeYNative = 0;
void* g_pFbAddrNative = nullptr;
void* g_pLUTAddrNative = nullptr;

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
    g_pDataBufferNative = malloc(sz);
    if (!g_pDataBufferNative) return -1;
    
    memset(g_pDataBufferNative, 0x1e, sz);
    
    g_pBackBufferNative = malloc(0x4400); 
    if (!g_pBackBufferNative) return -1;
    
    char* pc = (char*)g_pBackBufferNative;
    *pc++ = 'U';
    for (int i = 0; i < 17407; i++) {
        double d = (double)((int16_t)statebuffer_table[i]);
        d = round((d * 124.0) / 65532.0);
        *pc++ = (d > 0.0) ? (char)d : 0;
    }
    
    return 0;
}

int native_init_framebuffer(int* fb_info) {
    const char* file = "/dev/fb0";
    g_nFbFdNative = open(file, O_RDWR);
    
    int bytes_per_pixel = fb_info[3] / 8;
    int line_length = fb_info[1] * bytes_per_pixel;
    int size = line_length * fb_info[2] * (fb_info[11] + 1);
    
    if (g_nFbFdNative < 0) {
        std::cerr << "Cannot open device" << std::endl;
        return -1;
    }
    
    uint8_t finfo[0x50] = {0}; 
    if (ioctl(g_nFbFdNative, 0x4602, finfo) == -1) {
        std::cerr << "Error reading fixed information" << std::endl;
        return -1;
    }
    
    uint32_t vinfo[0x40] = {0}; 
    if (ioctl(g_nFbFdNative, 0x4600, vinfo) == -1) {
        std::cerr << "Unable to read screeninfo" << std::endl;
        return -1;
    }
    
    vinfo[0] = fb_info[1]; 
    vinfo[1] = fb_info[2]; 
    vinfo[2] = fb_info[2] * (fb_info[11] + 1); 
    vinfo[3] = fb_info[11] * fb_info[2]; 
    vinfo[4] = 0; 
    vinfo[5] = 0; 
    vinfo[6] = fb_info[3]; 
    vinfo[19] = fb_info[4]; 
    vinfo[20] = fb_info[5]; 
    vinfo[21] = fb_info[6]; 
    vinfo[22] = fb_info[7]; 
    vinfo[23] = fb_info[8]; 
    vinfo[24] = fb_info[9]; 
    vinfo[25] = fb_info[10]; 
    
    if (ioctl(g_nFbFdNative, 0x4601, vinfo) == -1) {
        std::cerr << "Error writing variable information" << std::endl;
        return -1;
    }
    
    g_pFbAddrNative = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, g_nFbFdNative, 0);
    
    if (g_pFbAddrNative == MAP_FAILED) {
        std::cerr << "Error: failed to map framebuffer device to memory" << std::endl;
        return -1;
    }
    
    g_nFbSizeXNative = line_length;
    g_nFbSizeYNative = fb_info[11] + 1; 
    
    return 0;
}

void native_init_lut_sub(uint32_t* param_1, int param_2) {
    uint32_t* puVar3 = param_1;
    uint32_t* puVar5 = param_1 + 0x104;
    while (puVar3 != puVar5) {
        puVar3[0] = 0x41000000;
        puVar3[1] = 0x41000000;
        puVar3[2] = 0x41000000;
        puVar3[3] = 0x41000000;
        puVar3 += 4;
    }
    
    uint32_t* p = param_1 + 7;
    do {
        p++;
        *p |= 0x200000;
    } while (p != param_1 + 18);
    
    uint32_t* puVar4 = param_1 + 55;
    do {
        puVar4[0] |= 0x20000000;
        puVar4[1] |= 0x20000000;
        puVar4[2] |= 0x20000000;
        puVar4[3] |= 0x20000000;
        puVar4 += 4;
    } while (puVar4 != param_1 + 255);
    
    if (param_2 == -1) return;
    
    uint32_t* p2 = param_1 + 26;
    while (p2 != puVar5) {
        p2[0] |= 0x100000;
        p2[1] |= 0x100004;
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
    
    for (int i = 0; i < 0x104; i++) {
        buf[i] = 0x43000000;
    }
    for (int i = 0x14; i <= 0x8d; i++) {
        buf[i] |= 0x40000;
    }
    for (int i = 0x28; i <= 0x65; i++) {
        buf[i] &= 0xfffdffff;
    }
    
    native_init_lut_sub(buf + 0x104, -1);
    native_init_lut_sub(buf + 0x208, 0); 
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
    
    for (int mode_idx = 0; mode_idx < mode_count; mode_idx++) {
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
            
            int out_size = 0;
            int i = 0;
            bool bVar1 = true;
            uint8_t curr = uVar8;
            do {
                if (ptr[0x29] == curr) {
                    bVar1 = !bVar1;
                } else {
                    int run = bVar1 ? wave_data[i+1] + 1 : 1;
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
                    int run = bVar1 ? wave_data[i+1] + 1 : 1;
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
            
            for (int iVar26 = 0; iVar26 < iVar27; iVar26++) {
                int iVar19 = 0;
                int iVar24 = iVar27;
                uint8_t* iVar29 = src + iVar26;
                do {
                    if (uVar14 != 0) {
                        uint32_t uVar15 = 0;
                        uint32_t uVar16 = 0;
                        while (uVar16 < uVar14) {
                            uint32_t uVar9 = uVar16 + 1;
                            uint32_t uVar1 = (uint32_t)iVar29[uVar16 * 0x400] << ((uVar16 & 7) * 2);
                            uint16_t uVar5 = (uint16_t)uVar15;
                            uVar15 = uVar15 | (uVar1 & 0xffff);
                            if ((uVar9 & 7) != 0 && uVar14 - 1 != uVar16) {
                                uVar16 = uVar9;
                                continue;
                            }
                            dest[(uVar16 / 8) * 0x401 + iVar19] = uVar5 | (uint16_t)uVar1;
                            uVar15 = 0;
                            uVar16 = uVar9;
                        }
                    }
                    iVar19++;
                    iVar29 += iVar27;
                } while (iVar19 != iVar24);
            }
            
            mode->luts.push_back(lut);
        }
        
        waveform_struct->push_back(mode);
    }
    
    return true;
}
