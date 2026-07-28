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

// update_data::flags bit values.
//
// Bit 0 (Sync) and bit 3 (ExplicitTemperature) are confirmed directly
// against queue_update's (0x3ccac) decompile: `bSync = flags & 1`, and
// `if (flags & 8) flTemperature = VectorSignedToFloat(data->zero, ...)` - an
// explicit caller-supplied temperature override, read from update_data's
// "zero" field (see swtcon_update's own comment on that field). Bit 2 (4) is
// never read anywhere in queue_update, so it's deliberately not given a
// name here.
//
// Bit 1 was originally named "FullRefresh" here (from this project's very
// first reversing pass, f16e890) - WRONG, and now corrected to FastDraw.
// queue_update itself only tests this bit as a raw boolean (no semantic
// clue), so the real evidence is one layer up, in the callers that build an
// update_data: EPFramebufferFusion::swapBuffers_impl (0x37d20) passes
// flags=2 to EPFramebufferSwtcon::updateInBlocks for its ENTIRE async
// "mode 0 fast UI + pen" branch (pixel_mode 6 or 7 depending on a
// *different*, higher-level EPFramebuffer::UpdateFlag bit that never
// reaches update_data itself) - and flags=1 (Sync only, never 2) for the
// two-pass synchronous full-screen clear the "Full Refresh" name was
// guessed from. So bit 1 tracks "fast/low-priority async draw" (matching
// RENDERING_PIPELINE.md's original, correct prose - "2: FastDraw... used to
// trigger pixel_mode=7 for pen strokes" - which the enum here just didn't
// match), not "do a full refresh." Also matches rm2fb's own IOCTL.cpp,
// which literally comments its analogous bit "// fast draw" for
// DU/partial-update pen traffic (though that's a different struct,
// UpdateParams, translated into pixel_mode rather than passed through - see
// Version3.20.cpp's mapUpdate). The WorkItem::fastDraw field name and every
// comment that used to justify this bit as "a full refresh redraws the
// whole area anyway" have been corrected to match - that reasoning was a
// plausible-sounding rationalization built on the wrong name, not
// independent evidence.
enum UpdateFlags {
    Sync = 1 << 0,
    FastDraw = 1 << 1,
    ExplicitTemperature = 1 << 3,
};

// Initializes the library and returns the 16-bit framebuffer pointer.
// dataBuffer/backBuffer optionally supply pre-allocated storage for the
// image working buffer / full-screen back buffer (e.g. rm2fb's shared
// framebuffer) instead of letting swtcon allocate its own - see
// init_statebuffer.
uint16_t* swtcon_init(void* dataBuffer = nullptr, void* backBuffer = nullptr);

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
