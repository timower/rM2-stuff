#pragma once

// A/B capture harness for the swtcon display pipeline (driver: ab_harness.cpp).
//
// Purpose: catch the class of bug that a settled-buffer comparison misses - a
// wrong TRANSIENT waveform frame (the per-phase drive data that settles the
// e-ink and is then overwritten in the 16-slot ring). That is exactly how the
// WorkItem.stateDataPtr (+0x44) rebasing bug hid: the state buffer and final
// framebuffer were byte-identical to the library, only the transient frames
// differed. See AGENTS.md.
//
// These hooks live on the real production code path (native_display.cpp) so the
// harness exercises exactly what ships. Every call is a cheap no-op unless
// SWTCON_AB_CAPTURE=<path> is set in the environment. Records are captured
// race-free on the display thread, buffered under a mutex, then (on flush)
// SORTED and written to <path>. Sorting makes the comparison order-independent:
// run the same input once native and once with SWTCON_LIBDISPATCH=1, then diff
// the two files (ab_harness --compare) - any divergence localizes to a line.

struct ListHead;
struct WorkItem;

// True if SWTCON_AB_CAPTURE is set (cached). The hooks below check this
// themselves; callers only need it to skip building expensive arguments.
bool ab_capture_enabled();

// Per surviving item, right after dispatch_processed_regions: the post-commit
// narrowed rect, sp3 (RegionRows) bounds, sp3 packed-buffer hash, and the
// stateDataPtr as an OFFSET (in u16) from sp3.dataPtr - never a raw pointer,
// which differs by allocation. This offset is what the +0x44 bug got wrong.
void ab_capture_dispatch(ListHead* sub_list);

// Per transient frame written by a playback-kernel advance, keyed by
// (seqId, absolute frame, slot) with a content hash. Called on the display
// thread right after the kernel returns, so no worker-pan race.
void ab_capture_playback(const WorkItem* item, int start_frame, int end_frame);

// Once per test step (after the batch settles): a hash of the whole state
// buffer and of all 17 frame slots - cheap regression sentinels.
void ab_capture_settled(const char* tag);

// Sort the buffered records and write them to SWTCON_AB_CAPTURE. Call after
// swtcon_shutdown (threads joined) so the buffer is quiescent. No-op if
// capture is disabled.
void ab_capture_flush();

// Compare two capture files (both produced by ab_capture_flush, already
// sorted). Prints a localized summary to stdout. Returns 0 if identical, 1 if
// they differ, 2 on I/O error. Used by ab_harness's --compare mode.
int ab_capture_compare(const char* path_a, const char* path_b);
