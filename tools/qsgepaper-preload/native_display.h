#pragma once

// Native reimplementation of the swtcon display pipeline - see AGENTS.md
// "Phase 5" and swtcon_architecture.md §6.1/§6.2/§6.4 for the reversing
// history/status. Both persistent threads are native now:
// native_worker_thread_func (the panel-driving frame-pacing loop) and
// native_display_thread_func (WorkItem/dependency-list state machine,
// GC, and the worker-side playback chain). dispatch_processed_regions
// (0x50660) and its two playback kernels (0x4a140/0x4a234) remain
// still-library by-address calls - dispatch_processed_regions hides a
// genuinely unreversed rectangle-merge algorithm, and the kernels' bodies
// are [derived]/[guess] (see §8's open questions) - so both are called by
// address exactly like render_update_kernel is in the update path.

// Mirrors worker_thread_func (0x3ae38): the panel-driving frame-pacing loop.
// Started by address today (kWorkerThreadFuncAddr in swtcon.cpp); this is
// its native replacement, same pthread entry-point signature.
void* native_worker_thread_func(void* arg);

// Mirrors FUN_0003b4b4 (0x3b4b4): requests the worker thread's flash
// sequence (native_worker_thread_func step 6) and blocks until it
// completes. Called by EPFramebufferSwtcon::initialize (0x38e30) right
// after qsgepaper_init - i.e. right after what's now swtcon_init - to flash
// the panel once on startup.
void native_request_flash_and_wait();

// Mirrors display_thread_func (0x3d2ac): the WorkItem/dependency-list state
// machine - see swtcon_architecture.md §6.2 for the full byte-verified
// breakdown. Started by address today (kDisplayThreadFuncAddr in
// swtcon.cpp); this is its native replacement, same pthread entry-point
// signature.
void* native_display_thread_func(void* arg);
