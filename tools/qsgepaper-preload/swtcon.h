#pragma once
#include <stdint.h>

struct update_data {
    int y0;
    int x0;
    int y1; // was misleadingly named "height" - it's the bottom-right corner's y, not a size
    int x1; // was misleadingly named "width" - it's the bottom-right corner's x, not a size
    int flags;
    int update_mode;
    int zero;
    int pixel_mode;
};

// Initializes the library and returns the 16-bit framebuffer pointer.
uint16_t* swtcon_init();

// Lock, update, unlock/post, and wait.
void swtcon_lock();
void swtcon_update(update_data* data);
void swtcon_unlock_post();
void swtcon_wait();

// Re-implemented natively
void swtcon_shutdown(int state_ptr_or_zero);

// Runtime load bias of libqsgepaper.so: ghidra_addr = runtime_pc - offset.
uintptr_t swtcon_runtime_offset();

// Debug: dump the loaded waveform LUTs (metadata + data checksum).
void swtcon_dump_waveform();

// Debug: checksum the fixed init tables (LUT, gamma, statebuffer).
void swtcon_dump_buffers();
