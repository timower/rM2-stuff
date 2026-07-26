#pragma once

// Native reimplementation of the swtcon display pipeline's worker thread -
// see AGENTS.md "Phase 5" and swtcon_architecture.md §6.1 for the reversing
// history/status. `display_thread_func` (§6.2) is still library code for
// now - it owns WorkItem/dependency-list state that worker_thread_func never
// touches, and hides a genuinely unreversed rectangle-merge algorithm
// (dispatch_processed_regions) - so it's deferred to a later pass.

// Mirrors worker_thread_func (0x3ae38): the panel-driving frame-pacing loop.
// Started by address today (kWorkerThreadFuncAddr in swtcon.cpp); this is
// its native replacement, same pthread entry-point signature.
void* native_worker_thread_func(void* arg);
