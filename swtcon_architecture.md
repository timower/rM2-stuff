# Swtcon / libqsgepaper Architecture

This is a reference for *how the swtcon (Software Timing Controller) subsystem
works* — data formats, control flow, algorithms — consolidated from reversing
`libqsgepaper_3.23.0.54.so`. It's a companion to `AGENTS.md`, not a
replacement: `AGENTS.md` tracks phase-by-phase porting *status* (what's
native, what's still library, what changed each round); this file describes
the *system*, native and still-library parts together, so a reader doesn't
have to reconstruct the architecture from a chronological changelog.

Confidence is marked inline for anything not byte-verified against the
library: **[confirmed]** (read directly from disassembly), **[derived]**
(consistent chain of reasoning from confirmed facts, not independently
checked), **[guess]** (plausible, unverified). Unmarked statements are
either native code (ours, so trivially "confirmed") or have been verified
byte-for-byte on hardware (per `AGENTS.md`). Two failure modes have already
bitten this project — a misread NEON vector-immediate, and a run-length
decode off-by-one — and both looked entirely plausible from static reading
alone; they only surfaced under byte-level A/B against the library. Treat
`[derived]`/`[guess]` accordingly before shipping native code against them.

Native source lives in `tools/qsgepaper-preload/`. `swtcon.h` is the public
API (`swtcon_init/update/lock/unlock_post/wait/shutdown`); `swtcon.cpp` owns
`dlopen`/`dlsym` loading and init/shutdown orchestration; `native_init.cpp`
owns init-allocated resources; `native_update.cpp` owns the update path;
`native_display.cpp` owns both persistent display-pipeline threads
(`native_worker_thread_func` and `native_display_thread_func`, Phase 5);
`qsgepaper_globals.h` models the library's own `.bss` layout we still read
by address.

---

## 1. Wire formats

These are the binary layouts the native code and the still-library code both
read/write, so they have to match exactly — libstdc++ ABI included (this
binary uses the **new** SSO `std::string` ABI, `_GLIBCXX_USE_CXX11_ABI=1`;
`tools/qsgepaper-preload` is pinned to match).

### SpRef — inlined shared_ptr

The library inlines `std::shared_ptr<T>` as two words:

```
struct SpRef { void* ptr; void* ctrl; };   // 8 bytes
```

`ctrl` points at a `_Sp_counted_base`-shaped control block: `vtable, useCount,
weakCount`. Retain/release are hand-rolled atomic increments/decrements on
`ctrl[1]`/`ctrl[2]` (`retain_sp`/`release_sp` in `native_update.cpp`) — on
zero, `vtable[2]` (`_M_dispose`) then `vtable[3]` (`_M_destroy`) get called
through the vtable.

### WorkItem — the queued update item (0x5c bytes)

Lives embedded at `WorkItemNode + 8`. Field order and meaning, current as of
this document (see `native_update.h` for the authoritative C++ definition —
some of the later fields below aren't renamed there yet, noted per-field):

| Offset | Field | Notes |
|---|---|---|
| `+0x00` | `SpRef regionRows` | `dispatch_update_regions`'s output; the raw-pointer half points **at the `RegionRows` struct itself** (not a separate object it merely references). |
| `+0x08` | `int32 gap` | Cached `regionRows.ptr→dataPtr`, **rebased to this item's own rect origin** — `native_piece_builder` maintains this on split via `stride*(piece.x0-old.x0) + (piece.y0-old.y0)`. |
| `+0x0c` | `int32 rectY0` | |
| `+0x10` | `int32 rectX0` | Rect field order is `{y0,x0,y1,x1}`, not `{x0,y0,x1,y1}`. |
| `+0x14` | `int32 rectY1` | |
| `+0x18` | `int32 rectX1` | |
| `+0x1c` | `int32 seqId` | Stamped from a global sequence counter (`kSeqCounterAddr`, 0x6d178) on every `update_item_ctor`/`native_piece_builder` call. |
| `+0x20` | `int32 frameCursor` | **[confirmed]** Next display-frame index this item's waveform should render into. Advances by 1 (worker's normal per-frame tick) or up to 8 (`advance_work_item_frames`'s batched advance). |
| `+0x24` | `int32 frameAnchor` | **[confirmed]** The frame this item's playback started on. `frameAnchor + lutWidthMinus1` is the last frame the item is still active on. |
| `+0x28` | `int16 phase` | **[confirmed]** Frames of this item's waveform already rendered. `lutWidthMinus1 − phase` = frames remaining. (Previously documented as "always observed 0" — that only held for single-item, non-overlapping test cases; it's a live counter once an item survives multiple frames.) |
| `+0x2a` | `int16 lutWidthMinus1` | Packed LUT width − 1, cached from the selected LUT. |
| `+0x2c` | `SpRef lut` | `shared_ptr<LUTEntry>`, the selected waveform LUT. |
| `+0x34` | `int16 mode` | Waveform mode (1–8). |
| `+0x36` | — | padding |
| `+0x38` | `float temperature` | |
| `+0x3c` | `SpRef sp3` | **[confirmed]** shape (upgraded from `[derived]` this pass): a *second* `RegionRows`-shaped buffer, holding this item's per-pixel **state** as `uint16` (distinct from `regionRows`, which holds `render_update_kernel`'s transition-byte *output*). `advance_work_item_frames` reads `sp3.ptr`'s `x0`/`x1` directly at `RegionRows`' own offsets (§6.4 step 5) — direct confirmation of the struct-shape claim, on top of the two unrelated kernel pairs (§6.3, §6.4) that independently converged on it earlier. Allocation site and the packed `uint16` payload's exact meaning remain open (§8). |
| `+0x44` | `void* stateDataPtr` | **[confirmed]** Cached `sp3.ptr→dataPtr`, same caching pattern as `gap` caches `regionRows.ptr→dataPtr`. Written by `dispatch_processed_regions` right after it allocates `sp3` (§6.2 step 4); the still-library playback kernels `FUN_0004a140`/`FUN_0004a234` read their per-pixel state through **this** pointer, not via `sp3.ptr→dataPtr` — leaving it stale was the sole cause of the `FUN_0004a234` "integration hazard" crash (§6.2 step 4). |
| `+0x48` | `ListHead intList` | Embedded `std::list<int>` head. Repurposed by `build_overlap_dependency_list` as an **overlap-dependency link list** — entries reference other in-flight `WorkItemNode`s this item's rendering needs to stay aware of (§6.2). |
| `+0x50` | `int32 intListCount` | |
| `+0x54` | `uint8 sync` | The `Sync` update flag. Selects `FUN_0004f8f0` (unset) vs `FUN_0004e680` (set) in the display-commit kernel (§6.3). |
| `+0x55` | `uint8 fullRefresh` | |
| `+0x56` | — | padding[2] |
| `+0x58` | `int32 pixelMode` | Default 5 ("auto" — resolved via `g_anPixelModeDispatchTable`, see §5.2). |

### RegionRows — the per-item pixel-diff output (0x1c bytes)

Allocated by `dispatch_update_regions` as a 0x28-byte block (shared_ptr
control block + this struct aliased at `+0xc`):

```
struct RegionRows {
  uint8_t* dataPtr;   // +0x00, size = stride*(x1-x0+1), column-major:
                       //   address(y,x) = dataPtr + stride*(x-x0) + (y-y0)
  int32_t y0, x0, y1, x1;  // +0x04 +0x08 +0x0c +0x10
  int32_t stride;     // +0x14, round_up(y1-y0+1, 16)
  int32_t size;        // +0x18, stride * (x1-x0+1)
};
```

### Other list/container shapes

- **`WorkItemNode`** — `{next, prev, WorkItem item}`, 100 bytes total (`operator_new(100)` throughout).
- **`BatchNode`** — `{next, prev, ListHead subList /*+0x08*/, int32 count /*+0x10*/, int16 mode /*+0x14*/}`, 0x18 bytes. The "claimed by worker" flag the worker thread sets lives at `+0x15` (exact byte-vs-whole-short boundary with `mode` unverified — kept as a raw offset read, `BatchNodeClaimed`, rather than a named field).
- **`IntListNode`** — `{next, prev, int32 value}`, 0xc bytes; nodes of `WorkItem::intList`.
- **`ModeEntry`** — `{std::string name /* 24 bytes, new ABI */, std::vector<shared_ptr<LUTEntry>> luts /* +0x18 */}`.
- **`LUTEntry`** — `{int size_kb, int mode_width, float temperature, int bit_depth, void* data}`.
- **Waveform struct** — `std::vector<ModeEntry*>` (`g_waveform_struct`/`waveformStructRaw`), one `ModeEntry*` per waveform mode.

---

## 2. Global state map

### `UpdateQueueGlobals` (base `0x66fd8`, modeled as a contiguous struct in `qsgepaper_globals.h`)

| Offset | Field |
|---|---|
| `0x66fd8` | `ListHead listProcessedUpdates` (same address as the flat Ghidra symbol `g_pListProcessedUpdates`) |
| `0x66fe0` | `int32 processedUpdatesCount` (`g_nProcessedUpdatesCount`) — item-count companion to `listProcessedUpdates`, same pattern as `accumCount`/`incomingBatchCount` below. |
| `0x66fe4` | `int32 curFrame` (`g_nCurFrame`) — **[confirmed]**, folded in this pass. The display frame index the worker thread is currently panned to; read/written under `displayTimingMutex` by the worker thread, read by `display_thread_func`'s frame-pacing computation. |
| `0x66fe8` | `int32 targetFrame` (`g_nTargetFrame`) — **[confirmed]**. The frame index the worker thread pans/catches-up toward; set by `display_thread_func`'s commit step (via `advance_work_item_frames`), read by the worker thread's cond-wait loop. |
| `0x66fec` | `pthread_mutex_t workerCondMutex` |
| `0x67008` | `pthread_cond_t workerCond` |
| `0x67038` | `uint8_t flashRequested` (`g_bFlashRequested`) — **[confirmed]** read/written as a single byte, not int32 as an earlier pass assumed. |
| `0x6703c` | `pthread_mutex_t displayTimingMutex` |
| `0x67058` | `Timespec64 lastPanTimestamp` (`g_lastPanTimestamp`) — **[confirmed]**, new finding. A Y2038-safe kernel timespec (two `int64`s, 16 bytes — the binary calls `__clock_gettime64(CLOCK_MONOTONIC_RAW, ...)`, not this toolchain's own possibly-32-bit-tv_sec `clock_gettime`). Stamped by the worker thread every time it sets `nLastPannedFrame`, always under `displayTimingMutex`; read by `display_thread_func`'s frame-pacing "elapsed time since last pan" computation. |
| `0x67068` | `sem_t displayThreadSem` |
| `0x67078` | `int32 timeVar` (`g_time_var`) |
| `0x6707c` | `int32 workerThreadShutdown` (`g_nWorkerThreadShutdown`) |
| `0x67080` | `uint8_t waveformStructRaw[12]` (`std::vector<ModeEntry*>`) |
| `0x6708c` | `int32 shutdownRequested` (`g_nShutdownRequested`) |
| `0x67090` | `ListHead listIncomingUpdates` |
| `0x67098` | `int32 incomingBatchCount` |
| `0x6709c` | `pthread_mutex_t updateQueueMutex` |
| `0x670b4` | `pthread_t displayThread` |
| `0x670b8` | `pthread_t workerThread` |
| `0x670bc` | `void* dataBuffer` (`g_pDataBuffer` — the "image" buffer, memset 0xff at init) |
| `0x670c0` | `void* backBuffer` (`g_pBackBuffer` — the "screen" buffer, calloc'd, ~0x281ac0 bytes) |
| `0x670c4` | `ListHead accumList` |
| `0x670cc` | `int32 accumCount` |
| `0x670d0` | `int16 accumFlag` |

All of the above are now folded into `UpdateQueueGlobals` in `qsgepaper_globals.h` (previously some were tracked as bare Ghidra names only).

### `FrameCursorGlobals` (base `0x66dd4`, separate contiguous cluster)

| Offset | Field |
|---|---|
| `0x66dd4` | `int32 nFrameCleanupCursor` (`g_nFrameCleanupCursor`) — `display_thread_func`'s own frame-housekeeping cursor; the worker thread never touches this one. |
| `0x66dd8` | `uint8_t bWorkerThreadBusy` (`g_bWorkerThreadBusy`) — **[confirmed]** byte flag, not int32. Set 1 at the top of each worker tick, cleared after the wait-for-work section. |
| `0x66ddc` | `int32 nLastPannedFrame` (`g_nLastPannedFrame`) — stamped to `curFrame - 1` by the worker thread alongside every `lastPanTimestamp` write (see the worker thread's own banner comment on why it's `curFrame - 1` and not `curFrame`); read by the display thread's stale-row cleanup upper bound and frame-pacing logic. |

The backBuffer per-frame-slot dirty-gate array (marked by `advance_work_item_frames`, read by `display_thread_func`'s stale-row cleanup) lives separately at `0x670d8`: 16 buckets (one per frame-slot ring position 0-15) × `0x57c` (1404, i.e. `SCREEN_WIDTH`) bytes each, one byte per column.

### Other named globals

| Name | Address | Role |
|---|---|---|
| `g_pGammaTable` | `0x6d1d4` | 128×0x88-byte table, indexed `[row&0x7f][col&0x7f]`. Despite the name, `[derived]` this is an ordered-dither threshold matrix, not a tone curve — see §5.2. |
| `g_dwTemperatureMutex` | `0x6d180` | Guards `g_flCachedTemperature`. |
| `g_flCachedTemperature` | `0x66e20` | Background-polled panel temperature (raw − 2.0°C). |
| `g_nPidFd` | `0x66dec` | |
| `g_anPixelModeDispatchTable` | `0x596b8` | 7-entry `int[]`, `render_update_kernel`'s auto-mode → case table (§5.2). |
| `g_pStateBuffer` | (native-allocated) | Column-major per-pixel state, stride `0x750`; read/written by the display-commit and worker-playback kernels (§6.3/§6.4). |

`g_nFrameCleanupCursor`/`g_bWorkerThreadBusy`/`g_nLastPannedFrame` (`FrameCursorGlobals`) and `g_nCurFrame`/`g_nTargetFrame`/`g_bFlashRequested`/`g_lastPanTimestamp`/`g_dwWorkerCond`/`Mutex` (`UpdateQueueGlobals`) are now folded into `qsgepaper_globals.h` — see the tables above, not repeated here.

### Thread-pool internals (see §7 — intentionally left `DAT_`-named)

Three independent `hardware_concurrency()`-sized pools exist (dispatch_update_regions's, dispatch_processed_regions's, FUN_0003ec78's). Each has its own one-time-init flag, thread-handle vector, task queue, mutex, and condvar — shapes are `[confirmed]`, but a few exact field boundaries (particularly the completion-tracking members) aren't, so these were deliberately left unrenamed in Ghidra rather than risk a wrong name.

---

## 3. Init flow (`swtcon_init`)

Fully native. In order: pid file → statebuffer/gamma table (`native_init_statebuffer`) → LUT (`native_init_lut`) → waveform load (`native_load_waveform`, full `.wbf` parser incl. RLE decompression) → framebuffer setup (`native_init_framebuffer`, wires `g_fbVarScreeninfo`/`g_fbFixScreeninfo`) → temperature sensor discovery + initial poll (`native_init_temperature_sensor`) → `native_pan_and_unblank`/`native_prime_display` to seed the frame counters → starts the two still-library threads (`worker_thread_func`, `display_thread_func`) by address.

Key facts that took real reversing effort (see `AGENTS.md` Phase 3 for the full bug history):
- Four separate buffers get wired: `dataBuffer`/"image" (memset 0xff), `backBuffer`/"screen" (calloc), `g_pStateBuffer` (`0x001e001e` pattern — **not** `memset(0x1e)`, which would make each 16-bit state `0x1e1e`), `g_pGammaTable`.
- The gamma table's source values are **unsigned** 16-bit and the library **pre-increments** the read pointer (skips the first entry).
- The waveform's `.wbf` RLE decode advances by **2** bytes per run (value + length), and the mode loop is **inclusive** of `mode_count`.
- `native_init_framebuffer` must populate the *global* `g_fbVarScreeninfo`/`g_fbFixScreeninfo` (not just a local struct) — the still-library `pan_to_frame` reads those globals directly for `FBIOPAN_DISPLAY`.

---

## 4. Update flow (`swtcon_update` / `queue_update`)

Fully native (`native_update.cpp`) — as of Phase 6, zero remaining by-address library calls anywhere in this path.

1. **`swtcon_lock`** — `pthread_mutex_lock(&updateQueueMutex)`.
2. **`swtcon_update`** — builds a fresh `WorkItem` (`native_update_item_ctor`: degenerate rect, 25°C default, `pixelMode=5`, empty list/shared_ptrs), reorders the caller's `x/y/width/height` into the clamp function's expected axis order and runs **`native_clamp_update_rect`**: an independent per-axis point-reflection through `(SCREEN_HEIGHT-1, SCREEN_WIDTH-1) = (1871, 1403)` — i.e. flips into the panel's 180°-rotated hardware frame — with the y-axis rounded to 8-row blocks (down for y0, up for y1). Rejects degenerate results. Then:
   - `native_dispatch_update_regions(item, dataBuffer, backBuffer)` — native, §5.1.
   - `native_select_waveform_lut(temperature, mode)` picks the LUT (§4.1 below), retains it into `item.lut`.
   - `native_update_lut_is_valid` sanity-checks it (non-null data, positive `size_kb`/`bit_depth`/`mode_width`).
   - `native_subtract_update_region` clips the new rect out of the pending accumulation list *and* every unlocked queued batch (§4.2 below).
   - Deep-copies the item (`native_update_item_copy`) onto the tail of the accumulation list.
3. **`swtcon_unlock_post`** — `native_build_update_batch` clones the accumulation list into a fresh `BatchNode`, hooks it into the incoming-updates list after any worker-claimed batches, frees the originals, resets the accumulation state, unlocks, `sem_post`s the display semaphore.
4. **`swtcon_wait`** — spins on `g_nShutdownRequested` / the incoming-batch list.

### 4.1 `select_waveform_lut` — temperature bucket selection

Each `ModeEntry::luts` vector is sorted ascending by `LUTEntry::temperature`. The algorithm scans from index 1, keeping the highest index whose threshold the target temperature still meets or exceeds, stopping at the first index whose threshold it falls short of — "last bucket not exceeding temp," defaulting to the last entry if temp exceeds every threshold. (A single-entry vector trivially resolves to index 0 via the same loop.) Falls back to an empty placeholder LUT (`native_make_empty_lut`, a native reimplementation of the tiny inline allocator at `0x408a8`) if the mode is out of range or has no LUTs.

### 4.2 `subtract_update_region` — AABB rectangle subtraction

The heaviest of the fully-native leaves. Per node in the target list: skip if no AABB overlap; else compute the intersection ("cut") rect. If `cut == old` (full containment), remove the node outright. Otherwise emit up to 4 leftover axis-aligned strips — **left, top, bottom, right, in that fixed order**, each only if non-empty:

- left: `{old.y0, old.x0, old.y1, cut.x0-1}` — if `old.x0 < cut.x0`
- top: `{old.y0, cut.x0, cut.y0-1, cut.x1}` — if `old.y0 < cut.y0`
- bottom: `{cut.y1+1, cut.x0, old.y1, cut.x1}` — if `cut.y1 < old.y1`
- right: `{old.y0, cut.x1+1, old.y1, old.x1}` — if `cut.x1 < old.x1`

Each piece is a full clone of the old item (preserving LUT/mode/temp/flags) with a new rect and a fresh sequence id (`native_piece_builder`), hooked in where the old node was.

---

## 5. `render_update_kernel` / `dispatch_update_regions` — Phase 6, now native

Both fully ported (`native_dispatch_update_regions`/`native_render_update_kernel` in `native_update.cpp`). The subsections below describe the *library's own* algorithm (allocation logic, formulas, addressing) as reversed and confirmed — the native port implements the same formulas/addressing (§5.2) as a single straightforward pass, deliberately **not** replicating the library's own thread-pool chunking (§5.1 point 4), since that chunking is provably invisible in the output (see §5.2's addressing box).

### 5.1 `dispatch_update_regions` (0x4fff8)

`(WorkItem* item, void* dataBuffer, void* backBuffer)`. In order:

1. Allocates the item's `RegionRows` blob (§1), releasing whatever `regionRows` shared_ptr the item had before.
2. **[confirmed]** If the rect is degenerate or narrower than 99 columns, skips threading — one synchronous call, `render_update_kernel(item, dataBuffer, backBuffer, chunkIndex=0, chunkCount=1)`.
3. **[confirmed]** Otherwise, lazily spins up (once, process-lifetime) a `hardware_concurrency()`-sized persistent worker pool — shared across every `dispatch_update_regions` call, **not** shared with the other two pools (§7).
4. **[confirmed]** Submits **exactly two** tasks (chunk count is a literal `2` baked into the task object, not `hardware_concurrency()`) — left-half/right-half by column (the chunk split is computed from `rectX0`/`rectX1`, not the row bounds) — and blocks until both finish. The pool's extra depth is headroom for *concurrent* dispatches, not finer splitting of one.

### 5.2 `render_update_kernel` (0x4e7b8) — the per-pixel diff kernel

`(WorkItem* item, void* dataBuffer, void* backBuffer, int chunkIndex, int chunkCount)`. Zero further callees — fully self-contained. Output goes through `item.gap` (the rebased `RegionRows::dataPtr`), one byte per pixel, column-major.

**Mode dispatch:** `item.pixelMode` selects the case directly, except value `5` ("auto"), which indexes `g_anPixelModeDispatchTable[item.mode - 1]` (7 entries) — **`[confirmed]`**, table bytes read directly out of the loaded library at Ghidra address `0x596b8`:

| `item.mode` | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| → case | 6 | 9 | 9 | 9 | 9 | 6 | 8 |

Case 6 is a bare alias of case 8. So under auto mode, waveform modes 2–5 run case 9's formula and modes 1/6/7 run case 8's; case 7's own formula is only reached via an explicit `pixelMode=7`.

**`pixelMode` is xochitl's `extraMode` — `[derived]`, cross-referenced against `Version3.20.cpp`.** `libs/rm2fb/Versions/Version3.20.cpp`'s `AddressInfo::mapUpdate()` sets an `extraMode` field per input source before calling into the library's update function; that value lands directly in `WorkItem::pixelMode`. Its own table (comment above `mapUpdate`, driven by observed xochitl behavior, predates this reversing project):

| Source | wave | flags | extraMode (`pixelMode`) |
|---|---|---|---|
| Refresh | 2 | 1 | 9 |
| UI (full) | 3 | 0 | 9 |
| UI (partial) / Progress | 3 or 1 | 0 | 6 |
| **Pen** | 1 | 2 | **7** |
| **Marker** | 1 | 2 | **7** |
| Pan / key-UI / Shape | 6 | 0 | 6 |

So pixel mode 7 (the `backBuffer`-gated dither) is specifically what the pen and marker tools use; mode 6 (unconditional dither, case 8 alias) is what panning and UI chrome use; mode 9 is full-screen refreshes. This matches the user-visible symptom this section was investigated for: dithering artifacts are seen exactly when drawing gray with the pen and when panning, because those are the two paths that explicitly select a gamma/dither formula instead of falling through waveform-mode auto-dispatch.

**Per-pixel formulas — `[confirmed]`, both statically and at runtime (see box below).** Shared terms: `src` is the 16-bit source pixel from `dataBuffer`, split into `lo5 = src&0x1f`, `mid6 = (src&0x7ff)>>5`, `hi5 = src>>0xb` (bitfield split confirmed; what each field physically represents — old level / target level / dither accumulator — is still `[guess]`); `gamma = g_pGammaTable[row&0x7f][col&0x7f]`.

```
case 7 (gated):     out = backBuffer[pixel]!=0 ? ((lo5+mid6+hi5+gamma)/125)*30 : 0x20  // 0x20 = skip sentinel
case 8 (and 6):     out = ((lo5+mid6+hi5+gamma)/125)*30                                // unconditional
case 9:              out = (((lo5+mid6+hi5)*15+gamma)/125) << 1                         // unconditional
case 0xd:             out = 0x1e   // [confirmed] flat fill, no source read — "clearing" mode
default (0xa/0xb/0xc): out = ((lo5+mid6+hi5) >> 3) << 1                                  // no gamma lookup
```

**Verification methodology (`tools/qsgepaper-preload/render_kernel_verify.cpp`):** these formulas were re-derived a second time directly from ARM disassembly at `0x4e7b8` (not just the Ghidra decompiler's pseudocode — the bitfield extraction, the `gamma` table add, and the div-by-125-then-scale sequences were read off the raw `and`/`ubfx`/`umull`/`lsr` reciprocal-division idiom), then checked at runtime against the real library function. This first pass deliberately sidestepped the addressing/180°-rotation logic (solved separately, see the box below): the tool calls the real `render_update_kernel` directly (same `{item, dataBuffer, backBuffer, chunkIndex=0, chunkCount=1}` convention `dispatch_update_regions` itself uses for a single-shot dispatch) on a `dataBuffer` filled with one uniform 16-bit value across a rect **exactly 128×128 pixels**. Over that rect the kernel's two `&0x7f` loop counters each sweep a full 0–127 residue class exactly once regardless of screen position or rotation phase, so all 16,384 `(row&0x7f, col&0x7f)` gamma-table cells get visited exactly once each, just in an unknown order — turning a "byte-for-byte positional match" (which would need the rotation solved) into a "sorted-multiset match" (which doesn't). Passed on all 16 pixelMode/mode combinations across a diagnostic sweep of the full 16-bit `src` space (261 values/case, `render-kernel-verify 251` on the emulator — pass `1` for an exhaustive but ~40-hour sweep). Also incidentally confirmed the gamma table's base-pointer quirk: `g_pGammaTable` points at the *raw* allocation including its leading `'U'` version-tag byte, so `gamma[0][0]` really is `'U'` (0x55), not the first real dither sample — already baked correctly into `native_init_statebuffer`'s table, just now confirmed load-bearing.

**`gamma`/`g_pGammaTable` is an ordered-dither threshold matrix, not a tone curve — `[derived]`, upgraded this pass.** Three independent facts converge:
1. It's indexed purely by pixel *position* (`row&0x7f`, `col&0x7f`, a 128×128 tile), not by pixel *value* — a real gamma/tone-response curve would be indexed by intensity, not screen coordinate.
2. Its source data (`tools/qsgepaper-preload/statebuffer_table.h`, 17,407 `uint16` values) is a static blob copied byte-for-byte from the library's binary (`file offset 0x496da`, per the commit that generated it) — not computed at runtime — and the values themselves are non-monotonic/pseudo-random (range 0–65532, no smooth progression). A tone curve would be smooth; a dither threshold matrix looks exactly like this.
3. Every formula that reads `gamma` *adds* it into the pre-quantization sum before an integer divide (e.g. case 8: `(lo5+mid6+hi5+gamma)/125`) — textbook ordered dithering, where a spatially-varying threshold shifts the rounding boundary per pixel to fake intermediate gray levels out of a small set of discrete output levels (`*30` → steps of 30). Case 0xa/0xb/0xc's dither-free formula (no gamma lookup, plain bit-shift quantization) is consistent with this too — those are presumably reached by pixel modes xochitl 3.20 doesn't use, and skip dithering entirely.

This also gives the `backBuffer` gate in case 7 a concrete role: `libs/rm2fb/Versions/Version3.20.cpp:38-39` independently calls this same buffer `getGrayBuffer()` with a comment guessing it's used "when extraMode == 7" — written before this reversing project and now confirmed exactly. For the pen/marker path, dithering is applied only where the caller's gray mask is set; elsewhere the kernel emits the `0x20` skip sentinel and leaves the pixel alone.

Not closed out: the individual physical meaning of `lo5`/`mid6`/`hi5` (old level vs. target level vs. some other per-pixel accumulator — the *that* they get dithered together is now clear, the *what each one is* isn't).

**Addressing/rotation — `[confirmed]`, solved empirically (`tools/qsgepaper-preload/render_kernel_addr_map.cpp`).** Rather than hand-decoding the chunked/NEON/rotated ARM pointer arithmetic, this tool finds, for a chosen output byte, exactly which single `dataBuffer`/`backBuffer`/gamma-table byte feeds it — by binary-searching over each buffer (setting a contiguous half to a distinct marker value and checking whether the output flips), which needs zero assumptions about locality or rotation direction since each output byte is a pure function of exactly one input byte per buffer. Confirmed at 10 output positions across 2 different rects (dataBuffer/backBuffer) and 5 positions (gamma), all matching exactly with no exceptions:

```
y_screen = rectY0 + row          // row = 0..(rectY1-rectY0), the RegionRows-local row index
x_screen = rectX0 + col          // col = 0..(rectX1-rectX0), the RegionRows-local col index
src_y    = (SCREEN_HEIGHT-1) - y_screen   // 1871 - y_screen
src_x    = (SCREEN_WIDTH-1)  - x_screen   // 1403 - x_screen
srcIdx   = src_y * SCREEN_WIDTH + src_x   // same row-major index into BOTH dataBuffer and backBuffer
gamma    = g_pGammaTable[(src_x & 0x7f) + (src_y & 0x7f) * 0x88]
output[col * stride + row] = formula(case, dataBuffer[srcIdx], backBuffer[srcIdx] != 0, gamma)
```

In other words: the whole framebuffer is read through a 180° rotation (`(y,x) -> (H-1-y, W-1-x)`) relative to the update rect's own coordinate space, and the gamma table is indexed by the *rotated* (source-space) position, not the destination position. `dispatch_update_regions`'s two-way thread-pool chunking (§5.1) only ever splits the column range into disjoint pieces of this same computation — chunking changes nothing about which output byte reads which input byte, so a single-pass native port doesn't need to replicate the threading at all to be byte-identical, only the formula/addressing above.

---

## 6. Display pipeline (Phase 5 — both persistent threads now native)

Two persistent threads, started from `swtcon_init`. Both are native now
(`native_display.cpp`): `native_worker_thread_func` (the panel-driving
frame-pacing loop) and `native_display_thread_func` (the WorkItem/
dependency-list state machine, GC, and worker-side playback chain), both
started by function pointer instead of by address. `dispatch_processed_regions`
(0x50660) is now native too (§6.2 step 4) and wired in at the real call site,
along with its two display-commit kernels (`FUN_0004f8f0`/`FUN_0004e680`, §6.3)
— all confirmed both by decompilation/probe and now by a full clean emulator
run with the native path live. The previous "integration hazard" (a
deterministic crash in the still-library `FUN_0004a234`) turned out to be a
stale `WorkItem.stateDataPtr` (+0x44), not anything about `FUN_0004a234`
itself — see §6.2 step 4. The two worker-side playback kernels
(`FUN_0004a140`/`FUN_0004a234`, §6.4) are now the only remaining by-address
calls in the whole display pipeline.

### 6.1 `worker_thread_func` — one iteration per displayed frame (native)

Infinite loop, ticks once per frame the panel displays. Every step below was
byte-verified against the disassembly (not just decompiler pseudocode),
including the `nLastPannedFrame = curFrame - 1` bookkeeping, which looks like
an off-by-one at first glance but is transcribed verbatim on purpose:

1. **Pre-frame housekeeping** — if unblanked: double-pan the init slot (frame 16, twice, back to back), then under `displayTimingMutex` stamp `lastPanTimestamp` (`clock_gettime(CLOCK_MONOTONIC_RAW, ...)`, widened field-by-field into the library's Y2038-safe `Timespec64`, not reinterpreted) and `nLastPannedFrame = curFrame - 1`.
2. **Wake the display thread** — unconditional `sem_post(displayThreadSem)`, once per tick (the wakeup driving §6.2).
3. **Periodic reprime** — every 60s (`(double)(timeVar + 60) <= now`), `timeVar` is recomputed fresh from `gettimeofday()` (not incremented) and `prime_display()` is called again (a recurring keepalive, not init-only as first assumed in Phase 3).
4. **Wait for work** — under `workerCondMutex`, a 3-second bounded `pthread_cond_timedwait` loop entered only if `curFrame == targetFrame`; breaks immediately on shutdown or a pending flash, otherwise loops until the frame target changes. On timeout (`ETIMEDOUT`), calls `blank_fb()` once and switches to an untimed `pthread_cond_wait` for the rest of the wait.
5. **Un-blank and advance** — if still blanked and no flash/shutdown pending: `pan_and_unblank(curFrame % 16)`, then under `displayTimingMutex` stamp timing, `nLastPannedFrame = curFrame - 1`, `curFrame += 1`.
6. **Flash sequence** (only if `flashRequested`) — the classic full-panel black/white/black flash: `select_waveform_lut(temp, mode=0)` (the fixed flash-waveform mode entry) → if valid: write a checkerboard prime pattern (`0x0000`/`0x5555`/`0xaaaa`) into frame slots 0–2 (`write_flash_prime_pattern`, confirmed identical to `native_init_lut`'s own fill algorithm, just writing into an existing buffer with a caller-supplied pattern instead of allocating with a fixed 0) → `pan_and_unblank` to `read_lut_packed_pixel(lut,0,0,0)`, then `pan_to_frame` through phases `1..lut->size_kb-1` (the LUT's `size_kb` field doubles as this special LUT's phase count) → `pan_to_frame(0)`, `pan_to_frame(16)`, `blank_fb()` → re-upload the real waveform LUT into slots 0–2 and `reset_statebuffer_neutral()` (both confirmed identical in shape to existing init-time code, just applied to an already-allocated buffer) — then unconditionally (valid or not) `release_sp` the selected LUT and clear `flashRequested`.
7. **Catch-up** — while `curFrame < targetFrame`: `pan_to_frame(curFrame % 16)`, stamp timing under `displayTimingMutex` (same `curFrame - 1`/`curFrame + 1` pattern as step 5), `sem_post(displayThreadSem)`, repeat.

**Confirmed on the emulator**: the full HQ/medium/clearing + overlap-update test suite produces clean `EXIT=0` throughout, now with *both* threads native (hardware re-confirmation still pending).

### 6.2 `display_thread_func` (0x3d2ac) — native (`native_display_thread_func`)

Runs once per `sem_wait` (posted by §6.1 step 2). Ported natively this pass,
re-derived directly from disassembly (not just decompiler pseudocode) rather
than from the earlier prose sketch below, which turned out to have two
outright errors — the gate-check filter (step 3) and the kernel-selection
rule in `advance_work_item_frames` (§6.4) — both now closed out exactly
rather than `[derived]`:

1. **Stale-row cleanup [confirmed, byte-exact]** — loop `for (i = nFrameCleanupCursor - 15; i <= nLastPannedFrame; i++)`, bucket = `i mod 16` (plain C truncating mod — negative buckets only reachable near startup, not fully resolved but the formula itself is exact). Per bucket: scan its 1404-byte (`SCREEN_WIDTH`) dirty-flag array (base `0x670d8 + bucket*0x57c`) byte-by-byte; each nonzero byte at column `c` triggers `copy_init_frame_row(frame_buffer_addr(bucket), c)` — confirmed at address **0x53be4** (not previously named): `memcpy(frame_slot_addr + (col+3)*0x410, g_pLUT + 0xc30, 0x410)`, i.e. it copies a fixed 0x410-byte reference row baked into the LUT blob into frame-slot row `col+3`. `copy_init_frame_row`'s own signature is `(void* frame_slot_addr, int col)`. After the scan, the whole 1404-byte bucket is zeroed and `nFrameCleanupCursor = i + 16`. Native (`native_copy_init_frame_row`/`native_stale_row_cleanup`).
2. **`g_pListProcessedUpdates` garbage collection [confirmed]** — teardown condition is exactly `curFrame >= frameAnchor + lutWidthMinus1`. Before freeing a doomed node, every *other* processed node's `intList` is scrubbed of any entry whose value (a `WorkItem*`, see `IntListNode`'s comment) equals the doomed node's item pointer — the matching teardown for the dependency links step 3 builds. The doomed node's own `intList`/shared_ptrs (`sp3`/`lut`/`regionRows`) are released and the node freed — this whole step decompiles to exactly `native_destroy_item_node`'s existing pattern from `native_update.cpp`, reused as-is by `native_gc_processed_updates`.
3. **Incoming-batch intake [confirmed]** — non-blocking trylock on `updateQueueMutex`, held for the rest of this tick's *entire* intake+dispatch+commit sequence across every batch in `listIncomingUpdates` — **not dropped before `dispatch_processed_regions`** as an earlier pass of this doc guessed; concurrent `swtcon_update()` calls simply block on the same mutex until this tick finishes. (`BatchNode`'s "claimed" byte at +0x15, per §1/§2, is never written anywhere in `display_thread_func` — it appears to be dead/vestigial given the mutex is held this broadly; still unconfirmed what if anything sets it.) Empty-sublist batches get unhooked/freed immediately. For non-empty batches:
   - `build_overlap_dependency_list(&batch->subList)` is called **twice** on the same batch — once here, once again after commit (step 5) once frame numbers are final. See §6.2a for the algorithm.
   - A **gate-check "max lifetime" scan** re-walks each item's freshly-built `intList`. **Now fully closed (was `[derived, not fully closed]`)**: a dependency entry is skipped — excluded from the max — exactly when `item.sync==0 && other.sync==0 && item.fullRefresh!=0 && other.fullRefresh!=0` (i.e. neither item requested `Sync`, and both are `FullRefresh` — waiting on another `FullRefresh`-only item's completion is pointless since a full refresh redraws the whole area anyway). Surviving dependencies contribute `other.frameAnchor + other.lutWidthMinus1`; the per-item max of that (0 if none survive) folds into a batch-wide `maxLifetime`.
   - **Gate:** `target = max(maxLifetime, curFrame)`; if `nFrameCleanupCursor - target > 6`, dispatch this batch now; else skip it this tick (retry next `sem_wait`), leaving it untouched in `listIncomingUpdates`.
   Native (`native_build_overlap_dependency_list`, gate-check inlined into `native_display_thread_func`).
4. **`dispatch_processed_regions` (0x50660) — fully reversed, natively reimplemented, and WIRED IN (`native_dispatch_processed_regions_native`/`native_commit_item`; confirmed on the emulator with the native path live — see "integration hazard, resolved" below).** Confirmed call signature: `bool dispatch_processed_regions(ListHead* subList)` — takes the batch's sub-list head directly, the same "count field aliases the caller's struct" trick `build_overlap_dependency_list` uses (it reads a count at `subList+2 words`, which for every real caller is the containing `BatchNode`'s own `count` field). Confirmed: a second, fully independent thread pool (own one-time-init flag, thread vector, task queue/mutex/condvar — none shared with `dispatch_update_regions`'s pool) — irrelevant to a native port for the same reason as `dispatch_update_regions`'s own pool (§5.1): it only splits the *same* per-item computation into disjoint column ranges, so a single-pass native port is byte-identical without replicating any threading.

   **Correction from an earlier pass:** decompilation of the batch-level bookkeeping (heap-allocated per-item "merge node" objects, dynamic vector growth, a byte-cost accumulator) looked at first glance like a genuine cross-item rectangle-merge algorithm, and was left by-address on that assumption. It is **not** — empirically verified this pass via `tools/qsgepaper-preload/dispatch_processed_regions_probe.cpp` (same "call the real function with controlled synthetic input, inspect what changed" technique as `render_kernel_addr_map.cpp`, since the vector-of-vectors bookkeeping is easy to mis-read by hand and a plain seed/narrow-direction slip would silently produce a wrong port). **Every item in the sub-list is processed completely independently — confirmed directly (probe experiment 6: two non-overlapping items in one batch, one all-unchanged one all-changed, dispatched together — item B's surviving rect came back as exactly its own original rect, entirely unaffected by item A).** The "merge node" per item is really just fresh backing storage for `item.sp3` (see below) plus scratch space to hold each of the item's own 1–2 *column-chunk* results before folding them together — not a merge across different `WorkItem`s. The chunk-count gate (1 vs 2) is on the item's own rect **X-span** (width) against a threshold of 29, not row-span/height as an earlier pass assumed, and since chunking only splits one item's own column range, it's provably invisible to a single-pass port exactly like `dispatch_update_regions`'s chunking (§5.1) — confirmed empirically too (probe experiment 7: same all-unchanged test with a 40-column rect forcing the 2-chunk path, identical destroyed-item outcome as the 1-chunk case).

   **Per-item algorithm (confirmed, both by decompilation and by the probe's empirical read-back of `g_pStateBuffer`/`sp3` afterward):**
   - If the item's rect is already degenerate (`rectY1<rectY0` or `rectX1<rectX0`): destroy the item outright (same node-teardown as `native_destroy_item_node`) — it is not dispatched and does not count toward the return value.
   - Otherwise: allocate a fresh `item.sp3` (releasing whatever it held before) — a `RegionRows`-shaped control block, but the payload is `uint16_t` per pixel (not a byte, unlike `item.regionRows`), sized `round_up(rows,16) * cols * 2` bytes, `stride` in the same "16-row-rounded" units. This resolves the "allocation site unconfirmed" note in `WorkItem::sp3`'s comment — it's allocated **here**, every dispatch, not at update time.
   - Run the commit kernel (`FUN_0004f8f0` if `item.sync==0` "incremental", `FUN_0004e680` if `sync!=0` "force" — picked once per item, not per batch as an earlier pass assumed) over the item's full original rect (a native port doesn't need to sub-chunk at all — see above). See §6.3 for the exact per-pixel formula this pass confirmed/corrected.
   - "Incremental" mode may **narrow** the item's rect to the tight bounding box of pixels that actually changed (Y rounds outward to the enclosing 8-row NEON lane group; X is exact, no rounding — confirmed via the probe's single-changed-pixel test: a lone change at rect-relative row 20 came back as Y range \[16,23\] rect-relative, X exact at the single changed column). If literally nothing in the whole item changed, the narrowed rect is degenerate and the item gets destroyed exactly like the upfront-degenerate case (confirmed via the probe's all-unchanged test, both 1-chunk and 2-chunk).
   - "Force" mode **never narrows** — confirmed both by decompilation (`FUN_0004e680` seeds `out_rect` once at the top and never touches it again in its pixel loop, unlike `FUN_0004f8f0` which conditionally updates it) and empirically (probe experiment 4). A force item's rect always survives unchanged.
   - Return value: `true` iff the sub-list ends up non-empty after processing every item (i.e. at least one item survived).

   **Integration hazard — RESOLVED; it was `WorkItem.stateDataPtr` (+0x44), not `FUN_0004a234`.** Wiring the native port in used to produce a **deterministic, 100%-reproducible SIGSEGV** inside `FUN_0004a234` (the still-library playback kernel formerly mislabeled "overlap-aware" in this doc - see §6.4's naming correction) on the very first HQ full-screen update — a few calls downstream of `dispatch_processed_regions` itself, not inside it. The cause: the real `dispatch_processed_regions` caches the freshly-allocated `sp3` buffer pointer into `WorkItem+0x44` (`stateDataPtr`) immediately after allocating `sp3` (disassembly: `node[0x13] = *piVar21`, i.e. `item.stateDataPtr = sp3.ptr->dataPtr`). **Both** playback kernels (`FUN_0004a140` *and* `FUN_0004a234`) read their per-pixel state through that cached +0x44 pointer (`*(item+0x44) + offset`), **not** through `sp3.ptr->dataPtr` — with the stride taken separately from `sp3.ptr->stride` (`*(item+0x3c)+0x14`). The native `native_commit_item` allocated `sp3` and set `sp3.ptr`/`sp3.ctrl` but left +0x44 stale, so the kernels dereferenced a stale/null pointer. Fixed by having `native_commit_item` set `item->stateDataPtr = block->rr.dataPtr` (matching the library). This also explains why the earlier elimination log below all came back negative — none of them touched +0x44:
   - **Not a downstream-state difference**: the field list checked (`frameCursor`/`frameAnchor`/`phase`/`rect`/`sync`/`lutWidthMinus1`) was byte-identical either way — but it did *not* include +0x44, which is exactly the field that differed.
   - **Not `sp3` buffer content**: zero-filling `sp3`'s payload couldn't help — the kernels never dereference `sp3.ptr->dataPtr`, only the cached `item.stateDataPtr`.
   - **Not timing**: an artificial 10ms/300ms delay didn't change the crash (correct — it's a pointer bug, not a race).
   - **Not a missed one-time global side effect**: priming with a real library call first didn't change it (correct — the missing write is per-item, per-dispatch, not a one-time global init).

   Verified on the emulator with the native path live: HQ/medium/clearing plus all five overlap tests run to completion with clean `EXIT=0`, continuous `FBIOPAN` traffic, and clean shutdown (hardware re-confirmation still pending). `FUN_0004a234` itself remains unreversed and still-library — it was never the culprit, just the innocent consumer of a pointer the native producer forgot to set.
5. **Commit / frame-pacing [now fully closed, corrects an earlier `[guess]`]** — on `dispatch_processed_regions` returning true:
   ```
   if (!bWorkerThreadBusy || curFrame != targetFrame) {
       workloadSum = Σ over non-degenerate items: (((x1-x0+1)*(y1-y0+1)) << 3) / 1000   // unsigned div
       budget = workloadSum + 100
       minX0  = min(item.rectX0) over the batch
       paceTarget = ((minX0 + 1) * 0x1d96) / 1000        // signed, truncating
       lock(displayTimingMutex)
         now = clock_gettime(CLOCK_MONOTONIC_RAW)         // widened the same way native_worker_thread_func writes it
         baseFrame = curFrame - 1 == nLastPannedFrame ? curFrame : curFrame - 1
         elapsed_us = (now - lastPanTimestamp) in microseconds
       unlock(displayTimingMutex)
       if (elapsed_us > 11762 /*0x2df2*/) { elapsed_us = 0; baseFrame += 1; }
       diff = paceTarget - elapsed_us
       if      (budget < diff)                   target = baseFrame
       else if (budget - diff <= 11761 /*0x2df1*/) target = baseFrame + 1
       else if (budget - diff <= 23523 /*0x5be3*/) target = baseFrame + 2
       else                                        target = targetFrame   // give up, snap
       target = min(target, targetFrame)
   } else {
       target = curFrame
   }
   committed = max(maxLifetime, target)   // maxLifetime from step 3's gate-check scan
   ```
   Every item in the batch gets `frameCursor = frameAnchor = committed`. (The apparent "separate pacing-cache fields" an earlier pass hypothesized at `UpdateQueueGlobals+0xc`/`+0x80` turned out to just be `curFrame` and `lastPanTimestamp` themselves, read under the mutex for cross-thread visibility — not distinct shadow fields.) Native, inlined into `native_display_thread_func`.
6. **After commit** — `build_overlap_dependency_list` runs again (dependencies now reflect final frame numbers), then per item: `advance_work_item_frames(item)` (§6.4), deep-copy (`update_item_copy`) onto a fresh node hooked onto `g_pListProcessedUpdates`, `processedUpdatesCount++`. The batch node is unhooked from `listIncomingUpdates`, its now-empty sub-list freed, and the `BatchNode` itself freed. Native.
7. **Bottom-of-loop sweep [confirmed]** — every node still in `g_pListProcessedUpdates` with `phase < lutWidthMinus1` gets another `advance_work_item_frames` call. Native.

**Exit:** both `g_pListProcessedUpdates` and `g_pListIncomingUpdates` empty *and* `shutdownRequested != 0` → thread exits.

**Confirmed on the emulator**: HQ/medium/clearing + the full overlap-update test suite (including the un-Sync burst-update test, which exercises the gate-check filter's `item.sync==0` path) produce clean `EXIT=0` against the native `display_thread_func`, with continuous `FBIOPAN` traffic throughout and a clean shutdown (hardware re-confirmation still pending).

### 6.2a `build_overlap_dependency_list` (0x3a838) — CONFIRMED, native

`void build_overlap_dependency_list(ListHead* subList)` — takes a batch's `WorkItemNode` sub-list head directly (not the batch struct itself). Per item in the list:

```
free every IntListNode in item.intList, reset to empty; item.intListCount = 0
if item's rect is degenerate (y0>y1 or x0>x1): continue   // skip entirely
for other in g_pListProcessedUpdates:
    if other's rect is degenerate: continue
    overlap = other.x0<=item.x1 && item.x0<=other.x1 && other.y0<=item.y1 && item.y0<=other.y1   // same AABB test as native_subtract_update_region
    if !overlap: continue
    if item.frameAnchor >= other.frameAnchor + other.lutWidthMinus1: continue   // strict < keeps the link - "other" must outlive item
    node = new IntListNode; node->value = (int32_t)(WorkItem*)&other_node->item   // a WorkItem*, NOT a scalar id - see IntListNode's comment
    list_insert_before(&item.intList, node)   // append at tail, same helper native_update.cpp already has
    item.intListCount++
```

Called twice per batch by `display_thread_func` (before dispatch and again after commit, on the same batch's sub-list).

### 6.3 Display-side commit kernels — `FUN_0004f8f0` / `FUN_0004e680` — **confirmed, both by decompilation and empirically**

`(WorkItem* item, int32_t out_rect[4], int chunkIndex, int chunkCount)`, called from §6.2 step 4. **[confirmed]** chunking is by column, not row, despite the row-span gate above — `out_rect` is written `{rectY1, endCol, rectY0, startCol}`, seeded once at the top of the function to the chunk's full extent and only ever narrowed (never widened) from there.

Per pixel (8-lane NEON, processed 8 rows of one column at a time): `new_raw` = the byte from `item.gap`'s buffer (the same output `render_update_kernel` produces, widened to u16, zero-extended); `old` = `g_pStateBuffer[col][row]` (u16); `is_sentinel = (new_raw == 0x20)` (the same skip sentinel `render_update_kernel` case 7 uses).

**Corrected this pass** (re-derived from `FUN_0004e680`'s disassembly, then cross-checked via `dispatch_processed_regions_probe.cpp` reading back the real `g_pStateBuffer`/`item.sp3` contents after a real library call — an earlier pass's `(state<<5)|value` guess had the wrong operand: it's the *old* state shifted, not the post-update one, and the "unchanged" marker is `0x0400`, not `0x0004` as an earlier nibble-swapped reading of the NEON fill constant `0x0400040004000400` claimed):

```
effective_new =
  FUN_0004f8f0 (sync==0, "incremental"): (is_sentinel || new_raw==old) ? old : new_raw
  FUN_0004e680 (sync!=0, "force"):        is_sentinel                  ? old : new_raw

skip (per-lane, decides the sp3 packed value only) =
  FUN_0004f8f0: is_sentinel || new_raw==old
  FUN_0004e680: is_sentinel   // NOTE: force does NOT skip on new_raw==old - it still
                              // recomputes the real (old<<5)|new_raw transition index
                              // even when new_raw happens to already equal old.

item.sp3[col][row] (u16, every pixel, unconditional) =
  skip ? 0x0400 : (old << 5) | effective_new
  // a transition-lookup index: old grayscale level packed with the new one,
  // not just the new state - matches a waveform-LUT lookup needing both endpoints.

g_pStateBuffer[col][row] and out_rect narrowing:
  FUN_0004f8f0 (incremental): only applied per 8-row NEON group, and only if AT
    LEAST ONE lane in that group of 8 has an actual (non-skip) change - if so, the
    WHOLE group's g_pStateBuffer cells get overwritten with their own effective_new
    (a no-op for the group's other unchanged lanes), AND out_rect's Y bound widens
    to cover the WHOLE 8-row group (not just the individual changed row) while the
    X bound narrows to the exact current column (no rounding). A group with zero
    real changes updates neither g_pStateBuffer nor out_rect at all.
  FUN_0004e680 (force): g_pStateBuffer[col][row] = effective_new, unconditionally,
    every pixel - out_rect is NEVER touched after its initial seed (confirmed: no
    min/max update code anywhere in FUN_0004e680's loop, unlike FUN_0004f8f0's).
```

Empirical confirmation (`dispatch_processed_regions_probe.cpp`, run against the real library on-target): a single differing pixel at rect-relative (row=20,col=5) within a 40×10 rect based at rectY0=100 narrowed the surviving item's rect to Y=\[116,123\] (exactly the enclosing 8-row group, `100 + 8*floor(20/8)` through `+7`) and X=\[205,205\] (exact). Reading back that pixel's `sp3` cell with old-state=9/new=5 gave `0x0125` = `(9<<5)|5` exactly; an unchanged neighbor in the *same* 8-row group (row=21) read back `0x0400`. A "force" pass with old=9/new=5 gave the identical `0x0125` sp3 packing and `g_pStateBuffer`=5 (=new), confirming both kernels share the same `(old<<5)|effective_new` packing formula.

### 6.4 Worker-side playback chain — native, kernel-selection rule corrected

`advance_work_item_frames` (0x3a984) → `FUN_0003f294`/`FUN_0003f1f0` → `FUN_0003ec78` → `FUN_0004a140` or `FUN_0004a234`. Called from `display_thread_func`'s commit step and bottom-of-loop sweep (§6.2 steps 6/7) — not from the worker thread. Everything down through `FUN_0003ec78` is native now (`native_advance_work_item_frames`/`native_dispatch_plain_kernel`/`native_dispatch_aligned_kernel`/`native_playback_kernel_dispatch` in `native_display.cpp`); only the two leaf kernels (`FUN_0004a140`/`FUN_0004a234`) stay by-address.

**Correction from an earlier pass:** the kernel-selection rule is *not* "`intList` empty → plain, non-empty → overlap-aware" — that was a plausible-looking guess from the high-level shape that turned out wrong once re-derived directly from disassembly. (That guess is also why this second kernel was originally named "overlap-aware" - a name this doc keeps below only to explain the history; the native port renames it "aligned", see the note after the rule.) The actual rule, confirmed byte-exact:
- If `intList` has an **active** dependency (one whose `frameAnchor + lutWidthMinus1` still exceeds this item's own `frameAnchor`): always the **"plain" kernel** (`FUN_0004a140`), regardless of phase alignment.
- Otherwise (`intList` empty, or every dependency has already expired): **`FUN_0004a234`** fires *only* if `phase` is 8-aligned (`phase % 8 == 0`); if not 8-aligned, it's still the plain kernel.

  i.e. this second kernel only fires on an 8-aligned phase boundary with no live dependency left to account for — the opposite of what "overlap-aware" (its name before this correction) suggests at a glance, and the reason the doc's original per-branch mapping was backwards for exactly the "empty `intList`, phase not yet 8-aligned" and "non-empty `intList`, active dependency" cases. **Renamed "aligned" throughout the native port** (`native_dispatch_aligned_kernel` in `native_display.cpp`, the `KERN` A/B-capture log tag, this doc) since the real distinguishing condition is the phase-alignment gate, not overlap-dependency state.

- **`advance_work_item_frames(WorkItem* item)` [confirmed]:**
  1. `frameCount = min(8 - phase%8, lutWidthMinus1 - phase)`.
  2. Computes 8 words `frame_buffer_addr((frameCursor+i) % 16)` for `i=0..7` — ring size is **16** frame slots (matches `FbInitParams.frameCount=0x10`), not 8 as an earlier pass assumed (8 is just how many slot addresses get precomputed per call).
  3. Picks a kernel per the corrected rule above. When the plain kernel is selected *and* `intList` is non-empty, first folds a tighter bound into `frameCount`: walks `intList` transitively (chases into each linked item's own dependencies, not a flat scan) folding a bound from `otherItem->frameCursor - 1` for every "settled" (its own `intList` empty, still `phase < lutWidthMinus1`) linked item, applied only when `phase == 0`. **Now fully closed** (was `[derived, needs A/B]`) — re-derived directly from disassembly this pass, see `native_advance_work_item_frames`'s implementation comment for the exact transcription.
  4. **Budget clamp:** `budget = nFrameCleanupCursor - frameCursor + 1`. On the plain-kernel path, `frameCount==0 || budget<frameCount` **aborts the whole call** (no kernel call, no tail — an early `return`, not just a skip). On the aligned-kernel path, `budget<frameCount` only skips the kernel call; the tail (step 6) still runs.
  5. After a kernel call, for every newly-advanced frame slot, marks dirty every byte in `[sp3.x0, sp3.x1]` of that slot's dirty-gate row (base `0x670d8 + slot*0x57c + x`, matching §6.2 step 1's array exactly) — the *producer* side of the stale-row cleanup this array feeds, and of `render_update_kernel` case 7's gate. Only runs if `frameCursor` actually advanced (guards against the LUT-wraparound correction in `FUN_0003ec78`'s commit consuming the whole advance).
  6. Tail (always reached except the plain-path early-abort in step 4): if `frameCursor < curFrame - 1`, logs a "generator thread has fallen behind" warning (cosmetic). If `frameCursor > targetFrame`: `targetFrame = frameCursor`, then `pthread_cond_broadcast(&workerCond)` under `workerCondMutex` — this is the only place `targetFrame` is written, and how it reaches the (native) worker thread's wait loop.
- **`FUN_0003f294`** (plain kernel wrapper) / **`FUN_0003f1f0`** (aligned kernel wrapper) — **[confirmed]** thin wrappers, signature `(void* frameSlots[8], WorkItem* item, int frameCount, int chunkIndex)` (chunkIndex always `0` from their only caller). Chunk count: default 1; if the rect is non-degenerate and `area = (x1-x0+1)*(y1-y0+1) > 20000`: `chunkCount = (x1-x0 < 10) ? 1 : 2`. Then `FUN_0003ec78(kernelFn, frameSlots, item, frameCount, chunkCount)` — the *only* difference between the two wrappers is which kernel pointer (`FUN_0004a140` vs `FUN_0004a234`) they pass. Native (`native_dispatch_plain_kernel`/`native_dispatch_aligned_kernel`/`native_playback_chunk_count`).
- **`FUN_0003ec78(kernelFn, frameSlots, item, frameCount, chunkCount)` [confirmed]** — the third independent thread pool. `chunkCount < 2`: calls `kernelFn(frameSlots, item, frameCount, /*chunkIndex=*/0, /*chunkCount=*/1)` synchronously (chunkCount forced to 1 even if the caller passed something else). `chunkCount >= 2`: lazily spins up (once, process-lifetime) a `hardware_concurrency()`-sized pool, submits exactly `chunkCount` closures `{kernelFn, frameSlots, item, frameCount, chunkIndex=0..chunkCount-1, chunkCount}`, blocks until idle. Native (`native_playback_kernel_dispatch`) — since `chunkCount` is always 1 or 2 here and nothing else in the binary reads this pool's own bookkeeping globals, the port just spins up one `std::thread` per chunk and joins, rather than replicating the library's persistent lazily-initialized pool 1:1. **Commit (runs once per call, after the synchronous call or after all chunks finish):**
  ```
  lutWidth = *(int*)item->lut.ptr          // full packed width, NOT lutWidthMinus1
  newPhase = item->phase + frameCount
  item->frameCursor += frameCount
  item->phase = newPhase
  if (lutWidth <= newPhase) {                // overflowed past the LUT's length
    excess = newPhase - lutWidth
    item->frameCursor -= excess
    item->phase -= excess
  }
  ```
  Unsynchronized plain field writes — no mutex held.
- **Kernel call signature [confirmed]:** both `FUN_0004a140` and `FUN_0004a234` are `void fn(void* frameSlots[8], WorkItem* item, int frameCount, int chunkIndex, int chunkCount)` — `FUN_0004a140`'s prologue jump-tables on `frameCount` (0–8) to one of 9 unrolled sub-implementations.
- **`FUN_0004a140`** ("plain" kernel) — `[derived]`. Reads a per-pixel state value from `item.stateDataPtr` (+0x44), indexes into a phase-specific LUT slice (same addressing `read_lut_packed_pixel` uses), and **ORs** the result into the destination frame-buffer word — a per-frame OR-accumulated waveform bit-plane (standard for e-ink multi-bit drive schemes). Case N touches exactly N consecutive frame slots. Exact NEON shift constants not term-verified.
- **`FUN_0004a234`** (formerly called "overlap-aware" here - corrected to "aligned", see §6.4) — body shape `[guess]` (never independently reversed - proven output-identical to `FUN_0004a140` instead, see AGENTS.md). Selected when the item has NO active overlap dependency left AND `phase` is 8-aligned (the opposite of the original "has overlap links" guess this entry made). `frameCount` cases 1–3 delegate to three further, **entirely unreversed** functions: `FUN_0004a3f8`, `FUN_0004a9e0`, `FUN_0004b098`. Common in practice (fires on the very first HQ full-screen update, per §8), not rare as originally guessed here.

### 6.5 Thread pool inventory

Three independent, lazily-spun-up, `hardware_concurrency()`-sized, process-lifetime thread pools exist in this binary — none shared with each other:

| Pool owner | Spun up from | Chunking rule | Serves |
|---|---|---|---|
| `dispatch_update_regions` | `swtcon_update`'s call into it | exactly 2, by column | `render_update_kernel` |
| `dispatch_processed_regions` (0x50660) | `display_thread_func` step 4 | 1 or 2 (merged-rect X-span<29 gate — corrected from an earlier "row-span" assumption), by column | `FUN_0004f8f0`/`FUN_0004e680` |
| `FUN_0003ec78` | `FUN_0003f294`/`FUN_0003f1f0` | area>20000px && width<10 → 1, else 2 | `FUN_0004a140`/`FUN_0004a234` |

---

## 7. Shutdown flow

Native (`native_close_fb`, `native_unlock_pid_file`, `native_free_LUT`,
`native_free_statebuffer`, etc.), draining both update-queue lists and
joining both threads (both native now - `pthread_join` doesn't care) before
releasing native buffers. See `AGENTS.md` Phase 2 for the initial port and
the exit-time-destructor double-free fix (the library's own
`destroy_waveform_struct` exit handler means the native waveform struct must
be an intentionally-leaked `std::vector`, not owned/freed natively).

---

## 8. Open questions

Everything not marked `[confirmed]` above, plus:

- **`render_update_kernel`'s bitfield semantics** (§5.2) — formulas and addressing are both `[confirmed]` now (statically and at runtime); `gamma`'s *role* (an ordered-dither threshold matrix, added pre-quantization) is `[derived]` with strong cross-file evidence, but what `lo5`/`mid6`/`hi5` individually represent physically is still open (doesn't block the native port, which just needs the formula, not the semantics).
- **`FUN_0004a140`'s exact NEON shift constants** (§6.4) — shape confirmed, not term-verified.
- **`FUN_0004a234` and its three delegates** (`FUN_0004a3f8`/`FUN_0004a9e0`/`FUN_0004b098`) — fully unreversed, still by-address. It's reached on the very first HQ full-screen update in the standard emulator test suite (phase=0 is always 8-aligned, and a brand-new item never has an active dependency, so the aligned-kernel path fires immediately). It was *previously* blamed for the `dispatch_processed_regions` native-port crash, but that was a stale `WorkItem.stateDataPtr` (+0x44) in the native producer, now fixed (§6.2 step 4) — `FUN_0004a234` itself was never at fault. Still the next big reversing target (a many-KB, 9-way jump-table-dispatched NEON kernel), but no longer a blocker.
- **`BatchNode`'s "claimed" byte** (+0x15, §1/§2) — confirmed this pass that `display_thread_func` never writes it (the mutex is held across the whole intake+dispatch sequence instead, see §6.2 step 3); still unknown what, if anything, ever sets it, or whether it's simply dead.
- **`FUN_00050660`'s completion-tracking globals** — pool shape confirmed, a few exact field semantics aren't (why they're left `DAT_`-named in Ghidra, §2); moot for the native port, which doesn't replicate the pool at all (§6.2 step 4).

The frame-pacing target formula (§6.2 step 5) is now fully closed out (exact
constants, not just shape) but still worth an A/B pass before depending on it,
same as the rest of this list. The efficient way to close these out is the
technique already used for the gamma/LUT/statebuffer tables: write a native
candidate from the formulas above, diff its output byte-for-byte against the
real library function on identical input via an A/B dump harness, and let
mismatches point at whichever guess is wrong — not more static reading.

---

## 9. Function reference

Native reimplementations (see `native_init.cpp`/`native_update.cpp`/`swtcon.cpp` for the `native_*` counterparts):

| Library function | Address | Status |
|---|---|---|
| `init_framebuffer`, `init_LUT`, `init_statebuffer`, `load_waveform`, `init_temperature_sensor`, `pan_and_unblank`, `prime_display`, `frame_buffer_addr`, `upload_lut_to_frame_slot` | Phase 3 | Native |
| `update_item_ctor`, `update_item_copy`, `clamp_update_rect`, `get_current_temperature`, `free_update_region_list` | Phase 4b | Native |
| `subtract_update_region` (0x3be10), `build_update_batch` (0x3ea98), `FUN_000400a8`/piece-builder | Phase 4b | Native |
| `select_waveform_lut` (0x4535c), `update_lut_is_valid` (0x409e4) | Phase 4b | Native |
| `dispatch_update_regions` (0x4fff8), `render_update_kernel` (0x4e7b8) | Phase 6 | Native (`native_dispatch_update_regions`/`native_render_update_kernel`) |
| `worker_thread_func` (0x3ae38) | Phase 5 | Native (`native_display.cpp`) |
| `pan_to_frame` (0x53fxx, unconditional pan variant) | Phase 5 | Native (`native_pan_to_frame`) |
| `write_flash_prime_pattern` (0x53c04) | Phase 5 | Native (`native_write_lut_pattern`, shares `native_init_lut`'s fill body) |
| `reset_statebuffer_neutral` (0x4fbe0) | Phase 5 | Native (`native_reset_statebuffer_neutral`) |
| `read_lut_packed_pixel` (0x40c58) | Phase 5 | Native (`native_read_lut_packed_pixel`) |

The update path (`swtcon_update`) has zero remaining still-library calls as of Phase 6.

Still library, display pipeline:

| Function | Address | Role |
|---|---|---|
| `FUN_0004a140` | 0x4a140 | "Plain" per-frame OR-accumulated waveform bit-plane kernel - call signature confirmed, body `[derived]` |
| `FUN_0004a234` | 0x4a234 | "Aligned" kernel variant (formerly mislabeled "overlap-aware" - see §6.4); delegates to 3 unreversed functions for frameCount 1–3 - call signature confirmed, body `[guess]` (proven output-identical to `FUN_0004a140`, see AGENTS.md). (Previously blamed for the `dispatch_processed_regions` native-port crash; that was a stale `WorkItem.stateDataPtr` in the native producer, now fixed — §6.2 step 4.) |
| `FUN_0004a3f8` / `FUN_0004a9e0` / `FUN_0004b098` | 0x4a3f8 / 0x4a9e0 / 0x4b098 | Unreversed; `FUN_0004a234`'s delegates |

Native (Phase 5) - see `native_display.cpp`:

| Function | Address | Role |
|---|---|---|
| `worker_thread_func` | 0x3ae38 | Panel-driving frame-pacing loop - see §6.1. Ported natively; no longer started by address |
| `write_flash_prime_pattern` | 0x53c04 | Shares `native_init_lut`'s fill body, parameterized by dest+pattern |
| `read_lut_packed_pixel` | 0x40c58 | Generic bit-unpacking read of one packed pixel from a LUTEntry |
| `reset_statebuffer_neutral` | 0x4fbe0 | Reapplies the `0x1e001e` statebuffer fill |
| `display_thread_func` | 0x3d2ac | The WorkItem/dependency-list state machine - see §6.2. Ported natively this pass (`native_display_thread_func`); no longer started by address |
| `build_overlap_dependency_list` | 0x3a838 | Builds/clears an item's overlap-dependency `intList` against `g_pListProcessedUpdates` - see §6.2a (`native_build_overlap_dependency_list`) |
| `advance_work_item_frames` | 0x3a984 | Advances one item by up to 8 (of 16 ring) frames; marks the backBuffer dirty gate - see §6.4 (`native_advance_work_item_frames`) |
| `copy_init_frame_row` | 0x53be4 | Restores one stale frame-slot row from the LUT blob's fixed reference row (`native_copy_init_frame_row`) |
| `FUN_0003f294` / `FUN_0003f1f0` | 0x3f294 / 0x3f1f0 | Thin wrappers into `FUN_0003ec78` - see §6.4 (`native_dispatch_plain_kernel`/`native_dispatch_aligned_kernel`) |
| `FUN_0003ec78` | 0x3ec78 | Third thread pool; commits item phase/frameCursor after the kernel runs - see §6.4 (`native_playback_kernel_dispatch`), one `std::thread` per chunk instead of the library's persistent pool |
| `dispatch_processed_regions` | 0x50660 | Per-item degenerate check, `sp3` allocation (incl. the `stateDataPtr`/+0x44 cache), commit-kernel dispatch, rect narrow/destroy - see §6.2 step 4 (`native_dispatch_processed_regions_native`), single full-rect pass instead of the library's own thread pool + column-chunking. Wired in this pass; the by-address version is kept as `native_dispatch_processed_regions` for A/B. |
| `FUN_0004f8f0` / `FUN_0004e680` | 0x4f8f0 / 0x4e680 | Display-side commit kernels (incremental vs. force) - see §6.3 for the confirmed per-pixel formula (`native_commit_item`) |

## 10. Global reference

| Name | Address | Status |
|---|---|---|
| `g_nLastPannedFrame` | 0x66ddc | Folded into `FrameCursorGlobals` this pass |
| `g_bWorkerThreadBusy` | 0x66dd8 | Folded into `FrameCursorGlobals` this pass (confirmed byte flag, not int32) |
| `g_nFrameCleanupCursor` | 0x66dd4 | Folded into `FrameCursorGlobals` this pass |
| `g_bFlashRequested` | 0x67038 | Folded into `UpdateQueueGlobals` this pass (confirmed byte flag, not int32) |
| `g_nProcessedUpdatesCount` | 0x66fe0 | Folded into `UpdateQueueGlobals` this pass |
| `g_nCurFrame` | 0x66fe4 | Folded into `UpdateQueueGlobals` this pass - new finding, was inside a "reserved" gap |
| `g_nTargetFrame` | 0x66fe8 | Folded into `UpdateQueueGlobals` this pass - new finding, was inside a "reserved" gap |
| `g_lastPanTimestamp` | 0x67058 | Folded into `UpdateQueueGlobals` this pass as `Timespec64` - new finding (Y2038 `__clock_gettime64` ABI, two int64s) |
| backBuffer dirty-gate array | 0x670d8, 16×0x57c | New finding this pass - per-frame-slot-ring-position dirty column bitmap, see §2 |
| `g_anPixelModeDispatchTable` | 0x596b8 | Already named prior to this project |
| `g_pGammaTable`, `g_flCachedTemperature`, `g_dwTemperatureMutex`, `g_abTemperatureHwmonPath` | 0x6d1d4 / 0x66e20 / 0x6d180 / — | Phase 3 |
| `g_pListProcessedUpdates`, `g_pListIncomingUpdates`, `g_nShutdownRequested`, `g_dwWorkerCond`/`Mutex`, `g_nWorkerThreadShutdown`, `g_time_var` | various | Already folded into `qsgepaper_globals.h` prior to this pass |
