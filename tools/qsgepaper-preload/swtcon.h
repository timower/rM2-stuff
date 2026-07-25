#pragma once
#include <stdint.h>

struct update_data {
    int y;
    int x;
    int height;
    int width;
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
