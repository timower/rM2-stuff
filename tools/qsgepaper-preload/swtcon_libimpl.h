#pragma once

// Runtime toggle (SWTCON_LIBIMPL env var) that switches every public
// swtcon_* entry point (init/lock/update/unlock_post/wait/shutdown) between
// the native reimplementation and the real library's own by-address
// exports. Test-only: lets a reference binary drive the unmodified library
// end-to-end for framebuffer/pan-dump A/B comparison against the native
// path, without needing the two to share any in-process state (unlike the
// old, now-defunct SWTCON_LIBDISPATCH mid-pipeline splice - see CLAUDE.md
// Phase 7). Not used by the production path.
bool
swtcon_lib_impl_enabled();
