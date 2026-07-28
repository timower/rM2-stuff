#pragma once

#include "native_update.h" // WorkItem

// Native reimplementation of the swtcon display pipeline - see AGENTS.md
// "Phase 5" and swtcon_architecture.md §6.1/§6.2/§6.3/§6.4 for the reversing
// history/status. Both persistent threads are native now:
// native_worker_thread_func (the panel-driving frame-pacing loop) and
// native_display_thread_func (WorkItem/dependency-list state machine and GC).
// dispatch_processed_regions (0x50660) and its two display-commit kernels
// (0x4f8f0/0x4e680) are fully reversed and now NATIVE too
// (native_dispatch_processed_regions_native/native_commit_item in
// native_display.cpp), wired in at the real call site. The previous
// "integration hazard" (a deterministic crash in the still-library
// playback kernel 0x4a234, formerly mislabeled "overlap-aware" - see
// native_dispatch_aligned_kernel's comment in native_display.cpp for why)
// was a stale WorkItem.transitionDataPtr (+0x44), not anything about 0x4a234
// itself - see native_commit_item and swtcon_architecture.md §6.2 step 4.
// Both worker-side playback kernels are native now too
// (native_playback_kernel_plain below backs both the "plain" and "aligned"
// dispatch paths - see AGENTS.md) - zero remaining by-address calls in the
// production pipeline.

// Mirrors worker_thread_func (0x3ae38): the panel-driving frame-pacing loop.
// Started by address today (kWorkerThreadFuncAddr in swtcon.cpp); this is
// its native replacement, same pthread entry-point signature.
void*
native_worker_thread_func(void* arg);

// Mirrors FUN_0003b4b4 (0x3b4b4): requests the worker thread's flash
// sequence (native_worker_thread_func step 6) and blocks until it
// completes. Called by EPFramebufferSwtcon::initialize (0x38e30) right
// after qsgepaper_init - i.e. right after what's now swtcon_init - to flash
// the panel once on startup.
void
native_request_flash_and_wait();

// Mirrors display_thread_func (0x3d2ac): the WorkItem/dependency-list state
// machine - see swtcon_architecture.md §6.2 for the full byte-verified
// breakdown. Started by address today (kDisplayThreadFuncAddr in
// swtcon.cpp); this is its native replacement, same pthread entry-point
// signature.
void*
native_display_thread_func(void* arg);

// Native reimplementation of both worker-side playback kernels (FUN_0004a140
// "plain" and FUN_0004a234, formerly mislabeled "overlap-aware" - it's
// actually an 8-phase-alignment fast path, not overlap-related at all, see
// native_dispatch_aligned_kernel in native_display.cpp - and proven
// byte-for-byte output-identical to the former, see AGENTS.md). For each
// column in
// [rectX0,rectX1] restricted to [chunkIndex,chunkCount)'s column sub-range,
// and each 8-row group in [rectY0,rectY1], looks up this call's waveform
// drive value per pixel and ORs it into up to `frameCount` frame slots - see
// the definition in native_playback_kernel.cpp (its own translation unit as
// of Phase 9, so its NEON fast path can sit behind an #ifdef) for the full
// byte-verified breakdown of the LUT-index and destination-address
// formulas. Non-static so tools/qsgepaper-preload/playback_kernel_bench.cpp
// can call it directly to measure the compute cost in isolation from the
// threading/dispatch code around it (native_playback_kernel_dispatch).
void
native_playback_kernel_plain(void** frame_slots,
                             WorkItem* item,
                             int frame_count,
                             int chunk_index,
                             int chunk_count);
void
native_playback_kernel_aligned(void** frame_slots,
                               WorkItem* item,
                               int frame_count,
                               int chunk_index,
                               int chunk_count);
