#pragma once

#include "update.h" // WorkItem

// Native reimplementation of the swtcon display pipeline - see AGENTS.md
// "Phase 5" and swtcon_architecture.md §6.1/§6.2/§6.3/§6.4 for the reversing
// history/status. Both persistent threads are native now:
// worker_thread_func (the panel-driving frame-pacing loop) and
// display_thread_func (WorkItem/dependency-list state machine and GC).
// dispatch_processed_regions (0x50660) and its two display-commit kernels
// (0x4f8f0/0x4e680) are fully reversed and now NATIVE too
// (dispatch_processed_regions_native/commit_item in
// display.cpp), wired in at the real call site. The previous
// "integration hazard" (a deterministic crash in the still-library
// playback kernel 0x4a234, formerly mislabeled "overlap-aware" - see
// dispatch_aligned_kernel's comment in display.cpp for why)
// was a stale WorkItem.transitionDataPtr (+0x44), not anything about 0x4a234
// itself - see commit_item and swtcon_architecture.md §6.2 step 4.
// Both worker-side playback kernels are native now too
// (playback_kernel_plain_intrinsics below backs both the "plain" and "aligned"
// dispatch paths - see AGENTS.md) - zero remaining by-address calls in the
// production pipeline.

// Mirrors worker_thread_func (0x3ae38): the panel-driving frame-pacing loop.
// Started by address today (kWorkerThreadFuncAddr in swtcon.cpp); this is
// its native replacement, same pthread entry-point signature.
void*
worker_thread_func(void* arg);

// Mirrors FUN_0003b4b4 (0x3b4b4): requests the worker thread's flash
// sequence (worker_thread_func step 6) and blocks until it
// completes. Called by EPFramebufferSwtcon::initialize (0x38e30) right
// after qsgepaper_init - i.e. right after what's now swtcon_init - to flash
// the panel once on startup.
void
request_flash_and_wait();

// Mirrors display_thread_func (0x3d2ac): the WorkItem/dependency-list state
// machine - see swtcon_architecture.md §6.2 for the full byte-verified
// breakdown. Started by address today (kDisplayThreadFuncAddr in
// swtcon.cpp); this is its native replacement, same pthread entry-point
// signature.
void*
display_thread_func(void* arg);

// Wakes worker_thread_func's suspend gate (g_suspendCond in display.cpp)
// without forcing framebuffer_globals()->nIsFbBlanked=1 the way the public
// swtcon_resume() does - see swtcon_resume()'s comment for why that force
// is needed for a real resume, and swtcon_shutdown()'s call site (the only
// caller) for why it must NOT do that same force on the shutdown path.
void
wake_suspend_gate();

// Native reimplementation of both worker-side playback kernels (FUN_0004a140
// "plain" and FUN_0004a234, formerly mislabeled "overlap-aware" - it's
// actually an 8-phase-alignment fast path, not overlap-related at all, see
// dispatch_aligned_kernel in display.cpp - and proven
// byte-for-byte output-identical to the former, see AGENTS.md). For each
// column in
// [rectX0,rectX1] restricted to [chunkIndex,chunkCount)'s column sub-range,
// and each 8-row group in [rectY0,rectY1], looks up this call's waveform
// drive value per pixel and ORs it into up to `frameCount` frame slots - see
// the definition in playback_kernel_intrinsics.cpp (its own translation unit as
// of Phase 9, so its NEON fast path can sit behind an #ifdef) for the full
// byte-verified breakdown of the LUT-index and destination-address
// formulas. Non-static so tools/qsgepaper-preload/playback_kernel_bench.cpp
// can call it directly to measure the compute cost in isolation from the
// threading/dispatch code around it (playback_kernel_dispatch).
void
playback_kernel_plain_intrinsics(void** frame_slots,
                                 WorkItem* item,
                                 int frame_count,
                                 int chunk_index,
                                 int chunk_count);
void
playback_kernel_aligned_intrinsics(void** frame_slots,
                                   WorkItem* item,
                                   int frame_count,
                                   int chunk_index,
                                   int chunk_count);

// Individual pieces of display_thread_func, non-static so each can be
// unit-tested directly - see display.cpp for the full comments.
bool
commit_item(WorkItem* item);
bool
dispatch_processed_regions_native(std::list<WorkItem>& sub_list);
void
stale_row_cleanup();
void
gc_processed_updates();
void
build_overlap_dependency_list(std::list<WorkItem>& sub_list);
int
playback_chunk_count(const WorkItem* item);
void
advance_work_item_frames(WorkItem* item);

// worker_thread_func's own frame-pacing helper (steps 7/10), non-static so
// its frame-slot-selection formula can be unit-tested directly - see its
// definition in display.cpp for the full comment.
void
pan_and_advance_frame(UpdateQueueGlobals* queue,
                      FrameCursorGlobals* cursor,
                      bool unblank);
