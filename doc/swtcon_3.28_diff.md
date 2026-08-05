# libqsgepaper 3.27 → 3.28 diff notes

Companion to `swtcon_3.27_diff.md` (3.23 → 3.27) and `swtcon_architecture.md`
(3.23 baseline). This is a **quick pass**, not the exhaustive treatment the
3.23→3.27 diff got — it covers the init/thread-spawn path and the core
render/dispatch/playback pipeline, confirms whether the 3.27 rewrite (32bpp
switch, pixelMode table, playback kernel algorithm) carried forward, and
stops there. Same confidence tags: **[confirmed]**, **[derived]**, **[guess]**.

Programs, both in the `rm-shutdown` Ghidra project:
- `libqsgepaper_3.27.1.0`
- `libqsgepaper_3.28.0.164.so`

**Ghidra state for `libqsgepaper_3.28.0.164.so` reflects this pass** — every
function identified below with a confirmed 3.27 counterpart has been renamed
to match (via `rename_function_by_address`): `swtcon_init` (`0x3e7d0`),
`display_thread_func` (`0x3dafc`, manually created — see below),
`advance_work_item_frames` (`0x3b2ec`), `dispatch_processed_regions`
(`0x4c8e0`), `dispatch_update_regions` (`0x4c420`), `render_update_kernel`
(`0x4a90c`), `dispatch_overlap_kernel` (`0x50e24`), `dispatch_plain_kernel`
(`0x50ec8`), `playback_kernel_plain` (`0x53df0`, manually created),
`playback_kernel_overlap` (`0x53ee4`, manually created). Globals:
`g_pSwtconState` (`0x62164`), `g_anPixelModeDispatchTable` (`0x5bae8`).
The "vsync-flip" thread body (3.27's `FUN_0003c454` → 3.28's `FUN_0003ba2c`)
was **not** renamed, since it was never named in the 3.27 pass either.

**Headline: the entire 3.27 rewrite (32bpp pixel decode, gamma-table
layout, pixelMode dispatch table, playback kernel algorithm) carries
forward to 3.28 unchanged.** What actually changed in 3.28 is narrower and
concentrated in `swtcon_init`: loss of the real `swtcon_init` exported
symbol, a new mutex guarding most of init, and growth of the global state
blob. The binary also shrank overall (~50 fewer functions, ~19KB smaller),
reversing the growth trend from 3.23→3.27.

---

## 1. Symbol/size changes — **[confirmed]**

| | 3.27 | 3.28 |
|---|---|---|
| Function count | 1902 | 1853 |
| Total memory size | 359818 bytes | 340782 bytes |
| `swtcon_init` exported symbol | present (`search_functions("swtcon_init")` hits) | **gone** — init function (`0x3e7d0`) is anonymous |

Opposite direction from 3.23→3.27, which grew. Not yet root-caused (could be
compiler/toolchain change, dead code removal, or inlining differences) —
out of scope for this quick pass.

## 2. `swtcon_init` — new mutex, bigger state blob, dropped a call — **[confirmed]**

- `depth_ = 1` (32bpp) and the `operator_new(0xa06b00)` primary-buffer size
  are both unchanged from 3.27 — the 32bpp switch is permanent, not a 3.27
  one-off.
- The global state blob grew another `+0x18` (24 bytes): `operator_new(0x58e8)`
  in 3.28 vs `operator_new(0x58d0)` in 3.27 (which was itself already
  `+0x18` over 3.23's `0x58b0`... consult `swtcon_3.27_diff.md` §5e for that
  earlier shift). The `condition_variable` constructor call moved from
  offset `0x5878`→`0x5890` accordingly.
- **New in 3.28**: a `pthread_mutex_lock`/`pthread_mutex_unlock` pair now
  wraps almost the entire body of `swtcon_init`, guarding the state blob at
  `state+0x5860`. No equivalent locking exists anywhere in 3.27's
  `swtcon_init`. Unconfirmed why — possibly hardening against concurrent
  `swtcon_init` calls, or a symptom of whatever added the missing call below.
- `EPFramebufferSwtcon::initialize` (`0x38418`) is structurally identical to
  3.27's wrapper, **except** it no longer makes a follow-up call after the
  init-success check — 3.23 called `init_request_flash()` there, 3.27 called
  `FUN_0003cd60()`; 3.28 has neither, going straight to the depth_ branch.
  Not yet chased further.

## 3. Thread layout — same two threads, one previously-undocumented one identified — **[confirmed]**

`swtcon_init` still spawns exactly two named threads, matching 3.27:

| Thread name | 3.27 body | 3.28 body | Note |
|---|---|---|---|
| `"vsync-flip"` | `FUN_0003c454` | `FUN_0003ba2c` (fuzzy 0.53) | Neither named in either version |
| `"framegen"` | `display_thread_func` (`0x3e62c`) | `display_thread_func` (`0x3dafc`, fuzzy 0.64) | |

**Correction to prior assumption**: `display_thread_func` is the
**`"framegen"`** thread body, not `"vsync-flip"` — confirmed directly from
`swtcon_init`'s decompile in both versions (the `pthread_setname_np` call
sites are unambiguous). `advance_work_item_frames` (`0x3bf5c`/`0x3b2ec`,
fuzzy 0.77) is called from inside one of these two thread bodies rather
than spawned as its own thread — consistent with its name (it's invoked
periodically, not a standalone worker loop).

`0x3dafc` had to be manually created via `create_function` before it could
be decompiled/fuzzy-matched or renamed — same situation as 3.27's playback
kernels, since it's only reached via a function pointer stored in a
`std::thread` state object, not a direct call.

## 4. `render_update_kernel` — pixel decode, gamma table, dispatch table: all unchanged — **[confirmed]**

Found by call-graph anchoring: `swtcon_update`'s wrapper (3.27
`FUN_0003e068` → 3.28 `FUN_0003d544`, fuzzy 0.51) calls
`dispatch_update_regions` (3.27 `0x4cc48` → 3.28 `0x4c420`, corroborated via
call-graph since direct fuzzy match on this pair alone returned nothing
above threshold), which calls `render_update_kernel` (3.27 `0x4afe4` → 3.28
`0x4a90c`).

Decompiling `0x4a90c` confirms, byte/constant-for-constant with 3.27:

- Same BT.601 luma decode for 32-bit input:
  `((px&0xffffff)>>0x10)*0x4d + ((px&0xffff)>>8)*0x96 + (px&0xff)*0x1d`,
  same shift/mask afterward.
- Same gamma-table addressing: `(&gammaTable)[uVar14*0x48 + (uVar24&0x3f)]`
  — stride `0x48`, mask `0x3f`, matching 3.27 (not 3.23's `0x88`/`0x7f`).
- Same switch-case structure: case 7 gated behind an early-exit check, case
  8 unconditional, case `0xc` (the case 7/8-role-swap target from 3.27 §6),
  case `0x12` (clear-to-gray, `0x1e`), default falling through to the
  same generic path.
- Same auto-mode `pixelMode` sentinel/table: sentinel value `6`, table at
  a `DAT_*` address with raw bytes `{7, 0xc, 0xc, 0xc, 9, 7}` — **byte-
  identical** to 3.27's table (read directly via `inspect_memory_content`,
  not inferred from case labels).

No difference found anywhere in this function beyond address relocation.

## 5. Playback kernels — algorithm unchanged, high-confidence fuzzy match — **[confirmed]**

`dispatch_processed_regions` (3.27 `0x4d2e0` → 3.28 `0x4c8e0`, fuzzy 0.36 —
weaker score, likely just more surrounding code churn, not algorithm
change) leads to `dispatch_plain_kernel`/`dispatch_overlap_kernel`
(3.27 `0x5229c`/`0x521f8` → 3.28 `0x50ec8`/`0x50e24`), identified by which
literal kernel address each one tail-calls into a shared threading helper
(`FUN_00050a8c`).

Those kernel addresses (`0x53df0`, `0x53ee4`) were undefined in Ghidra's
auto-analysis — same situation as 3.27, since they're only reached via a
function pointer, never a direct call — so they had to be manually created
via `create_function` before fuzzy matching or renaming would work.

Once created, fuzzy match against 3.27's already-confirmed playback
kernels came back unambiguous:

- `playback_kernel_plain` (3.27 `0x55150`) → 3.28 `0x53df0`, **score 0.869**
- `playback_kernel_overlap` (3.27 `0x55244`) → 3.28 `0x53ee4`, **score 0.882**

These are by far the highest fuzzy-match scores found in this whole pass —
strong evidence the playback kernels' actual per-pixel algorithm (LUT-index
formula, 2-bit packing, destination addressing) is unchanged from 3.27,
which itself was already confirmed unchanged from 3.23
(`swtcon_3.27_diff.md` §5e). Not case-by-case diffed beyond the fuzzy score
— if that's needed, follow the same decompile-and-diff approach used for
3.23→3.27.

---

## Next steps (not pursued in this quick pass)

1. Root-cause the ~50-function/~19KB shrinkage (§1) — toolchain change vs.
   dead code removal vs. inlining.
2. Decompile `FUN_0003ba2c` (`"vsync-flip"` thread body) — still unnamed in
   both 3.27 and 3.28, never covered by either diff pass.
3. Confirm what the new `swtcon_init` mutex (§2) actually protects against,
   and whether it's related to the dropped post-init call
   (`init_request_flash()`/`FUN_0003cd60()` in 3.23/3.27, absent in 3.28).
4. `WorkItem`/`LUTEntry` layout diff (3.27→3.28) — not checked at all this
   pass; `swtcon_3.27_diff.md`'s open items on this (§5e/Next-steps #4) are
   still open and now have a second unknown version stacked on top.
