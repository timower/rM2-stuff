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
`native_display.cpp` owns the worker thread (`native_worker_thread_func`,
Phase 5); `qsgepaper_globals.h` models the library's own `.bss` layout we
still read by address.

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
| `+0x3c` | `SpRef sp3` | **[derived]** A *second* `RegionRows`-shaped buffer, holding this item's per-pixel **state** as `uint16` (distinct from `regionRows`, which holds `render_update_kernel`'s transition-byte *output*). Independently converged on from two unrelated kernel pairs (§6.3, §6.4) — cross-validation is why this is `derived` rather than `guess`, though neither trace reached the allocation site. |
| `+0x44` | `void* stateDataPtr` | **[derived]** Cached `sp3.ptr→dataPtr`, same caching pattern as `gap` caches `regionRows.ptr→dataPtr`. |
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
| `g_pGammaTable` | `0x6d1d4` | 128×0x88-byte gamma table, indexed `[row&0x7f][col&0x7f]`. |
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

Fully native control flow (`native_update.cpp`), calling into two still-library leaves for the actual pixel work (§5).

1. **`swtcon_lock`** — `pthread_mutex_lock(&updateQueueMutex)`.
2. **`swtcon_update`** — builds a fresh `WorkItem` (`native_update_item_ctor`: degenerate rect, 25°C default, `pixelMode=5`, empty list/shared_ptrs), reorders the caller's `x/y/width/height` into the clamp function's expected axis order and runs **`native_clamp_update_rect`**: an independent per-axis point-reflection through `(SCREEN_HEIGHT-1, SCREEN_WIDTH-1) = (1871, 1403)` — i.e. flips into the panel's 180°-rotated hardware frame — with the y-axis rounded to 8-row blocks (down for y0, up for y1). Rejects degenerate results. Then:
   - `dispatch_update_regions(item, dataBuffer, backBuffer)` — still library, §5.1.
   - `native_select_waveform_lut(temperature, mode)` picks the LUT (§4.1 below), retains it into `item.lut`.
   - `native_update_lut_is_valid` sanity-checks it (non-null data, positive `size_kb`/`bit_depth`/`mode_width`).
   - `native_subtract_update_region` clips the new rect out of the pending accumulation list *and* every unlocked queued batch (§4.2 below).
   - Deep-copies the item (`native_update_item_copy`) onto the tail of the accumulation list.
3. **`swtcon_unlock_post`** — `native_build_update_batch` clones the accumulation list into a fresh `BatchNode`, hooks it into the incoming-updates list after any worker-claimed batches, frees the originals, resets the accumulation state, unlocks, `sem_post`s the display semaphore.
4. **`swtcon_wait`** — spins on `g_nShutdownRequested` / the incoming-batch list.

### 4.1 `select_waveform_lut` — temperature bucket selection

Each `ModeEntry::luts` vector is sorted ascending by `LUTEntry::temperature`. The algorithm scans from index 1, keeping the highest index whose threshold the target temperature still meets or exceeds, stopping at the first index whose threshold it falls short of — "last bucket not exceeding temp," defaulting to the last entry if temp exceeds every threshold. (A single-entry vector trivially resolves to index 0 via the same loop.) Falls back to an empty placeholder LUT (the tiny inline allocator at `0x408a8`) if the mode is out of range or has no LUTs.

### 4.2 `subtract_update_region` — AABB rectangle subtraction

The heaviest of the fully-native leaves. Per node in the target list: skip if no AABB overlap; else compute the intersection ("cut") rect. If `cut == old` (full containment), remove the node outright. Otherwise emit up to 4 leftover axis-aligned strips — **left, top, bottom, right, in that fixed order**, each only if non-empty:

- left: `{old.y0, old.x0, old.y1, cut.x0-1}` — if `old.x0 < cut.x0`
- top: `{old.y0, cut.x0, cut.y0-1, cut.x1}` — if `old.y0 < cut.y0`
- bottom: `{cut.y1+1, cut.x0, old.y1, cut.x1}` — if `cut.y1 < old.y1`
- right: `{old.y0, cut.x1+1, old.y1, old.x1}` — if `cut.x1 < old.x1`

Each piece is a full clone of the old item (preserving LUT/mode/temp/flags) with a new rect and a fresh sequence id (`native_piece_builder`), hooked in where the old node was.

---

## 5. The two remaining update-path leaves (still library)

### 5.1 `dispatch_update_regions` (0x4fff8)

`(WorkItem* item, void* dataBuffer, void* backBuffer)`. In order:

1. Allocates the item's `RegionRows` blob (§1), releasing whatever `regionRows` shared_ptr the item had before.
2. **[confirmed]** If the rect is degenerate or narrower than 99 columns, skips threading — one synchronous call, `render_update_kernel(item, dataBuffer, backBuffer, chunkIndex=0, chunkCount=1)`.
3. **[confirmed]** Otherwise, lazily spins up (once, process-lifetime) a `hardware_concurrency()`-sized persistent worker pool — shared across every `dispatch_update_regions` call, **not** shared with the other two pools (§7).
4. **[confirmed]** Submits **exactly two** tasks (chunk count is a literal `2` baked into the task object, not `hardware_concurrency()`) — top-half/bottom-half by row — and blocks until both finish. The pool's extra depth is headroom for *concurrent* dispatches, not finer splitting of one.

### 5.2 `render_update_kernel` (0x4e7b8) — the per-pixel diff kernel

`(WorkItem* item, void* dataBuffer, void* backBuffer, int chunkIndex, int chunkCount)`. Zero further callees — fully self-contained. Output goes through `item.gap` (the rebased `RegionRows::dataPtr`), one byte per pixel, column-major.

**Mode dispatch:** `item.pixelMode` selects the case directly, except value `5` ("auto"), which indexes `g_anPixelModeDispatchTable[item.mode - 1]` (7 entries):

| `item.mode` | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| → case | 6 | 9 | 9 | 9 | 9 | 6 | 8 |

Case 6 is a bare alias of case 8. So under auto mode, waveform modes 2–5 run case 9's formula and modes 1/6/7 run case 8's; case 7's own formula is only reached via an explicit `pixelMode=7`.

**Per-pixel formulas** — `[derived]` except case 0xd. Shared terms: `src` is the 16-bit source pixel from `dataBuffer`, split into `lo5 = src&0x1f`, `mid6 = (src&0x7ff)>>5`, `hi5 = src>>0xb` (bitfield split confirmed; what each field physically represents — old level / target level / dither accumulator — is `[guess]`); `gamma = g_pGammaTable[row&0x7f][col&0x7f]`.

```
case 7 (gated):     out = backBuffer[pixel]!=0 ? ((lo5+mid6+hi5+gamma)/125)*30 : 0x20  // 0x20 = skip sentinel
case 8 (and 6):     out = ((lo5+mid6+hi5+gamma)/125)*30                                // unconditional
case 9:              out = (((lo5+mid6+hi5)*15+gamma)/125) << 1                         // unconditional
case 0xd:             out = 0x1e   // [confirmed] flat fill, no source read — "clearing" mode
default (0xa/0xb/0xc): out = ((lo5+mid6+hi5) >> 3) << 1                                  // no gamma lookup
```

Not closed out: the physical bitfield meanings above, and some base-offset address arithmetic converting the chunked/180°-rotated iteration order to buffer addresses (internally consistent with `clamp_update_rect`'s rotation, not hand-verified term by term).

---

## 6. Display pipeline (Phase 5/6 — worker thread native, display thread still library)

Two persistent threads, started from `swtcon_init`. `worker_thread_func` is
now native (`native_display.cpp`'s `native_worker_thread_func`, started by
function pointer instead of by address); `display_thread_func` is still
started by address and remains 100% library.

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

**Confirmed on the emulator**: swapping in the native version and running the full HQ/medium/clearing + overlap-update test suite against the still-library `display_thread_func` produces clean `EXIT=0` throughout (hardware re-confirmation still pending).

### 6.2 `display_thread_func` (0x3d2ac) — the deep one (still library)

Runs once per `sem_wait` (posted by §6.1 step 2). Reversed in much greater depth this pass; several points below correct or close out gaps from an earlier, shallower read:

1. **Stale-row cleanup [confirmed, byte-exact]** — loop `for (i = nFrameCleanupCursor - 15; i <= nLastPannedFrame; i++)`, bucket = `i mod 16` (plain C truncating mod — negative buckets only reachable near startup, not fully resolved but the formula itself is exact). Per bucket: scan its 1404-byte (`SCREEN_WIDTH`) dirty-flag array (base `0x670d8 + bucket*0x57c`) byte-by-byte; each nonzero byte at column `c` triggers `copy_init_frame_row(frame_buffer_addr(bucket), c)` — confirmed at address **0x53be4** (not previously named): `memcpy(frame_slot_addr + (row+3)*0x410, g_pLUT + 0xc30, 0x410)`, i.e. it copies a fixed 0x410-byte reference row baked into the LUT blob into frame-slot row `row+3`. After the scan, the whole 1404-byte bucket is zeroed and `nFrameCleanupCursor = i + 16`.
2. **`g_pListProcessedUpdates` garbage collection [confirmed]** — teardown condition is exactly `curFrame >= frameAnchor + lutWidthMinus1`. Before freeing a doomed node, every *other* processed node's `intList` is scrubbed of any entry whose value (a `WorkItem*`, see `IntListNode`'s comment) equals the doomed node's item pointer — the matching teardown for the dependency links step 3 builds. The doomed node's own `intList`/shared_ptrs (`sp3`/`lut`/`regionRows`) are released and the node freed — this whole step is exactly `native_destroy_item_node`'s existing pattern from `native_update.cpp`, reusable as-is.
3. **Incoming-batch intake** — non-blocking trylock on `updateQueueMutex`. Empty-sublist batches get unhooked/freed immediately. For non-empty batches:
   - `build_overlap_dependency_list(&batch->subList)` is called **twice** on the same batch — once here, once again after commit (step 5) once frame numbers are final. See §6.2a for the algorithm.
   - A **gate-check "max lifetime" scan** re-walks each item's freshly-built `intList`, applying a filter that skips a dependency entry unless a `sync`/`fullRefresh` flag combination on both items holds — **`[derived, not fully closed]`**: the fields being read are confirmed as `WorkItem::sync`(+0x54)/`fullRefresh`(+0x55), but the exact byte offsets in this specific inlined copy weren't pinned down this pass. Per item this yields `max(lutWidthMinus1 + frameAnchor)` across surviving dependencies (0 if none survive); the batch-wide max of that becomes `maxLifetime`.
   - **Gate:** `target = max(maxLifetime, curFrame)`; if `nFrameCleanupCursor - target > 6`, dispatch this batch now; else skip it this tick (retry next `sem_wait`).
4. **`dispatch_processed_regions` (0x50660) — bigger and hairier than first assumed.** Confirmed: a second, fully independent thread pool (own one-time-init flag, thread vector, task queue/mutex/condvar — none shared with `dispatch_update_regions`'s pool). Dispatches to `FUN_0004f8f0` when the batch's *first item's* `sync==0`, else `FUN_0004e680` — picked once for the whole batch, not per item. **Correction from an earlier pass**: the bounding-box computation is *not* a simple union — decompilation shows a genuine interval/rectangle-merge algorithm (heap-allocated rect-merge nodes, dynamic vector growth/reallocation, a byte-cost accumulator, what looks like a full region-merge pass) that has **not** been reversed. The chunk-count gate (1 vs 2) is on the merged result's **X-span** (width) against a threshold of 29, not row-span/height as an earlier pass assumed. **Given this, `dispatch_processed_regions` should stay a still-library by-address call — same treatment as `dispatch_update_regions` — until its merge algorithm is actually understood; don't guess at a native replacement.** Blocks until its own pool finishes; return value is a "did anything actually get dispatched" flag (false = empty rect union).
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
   Every item in the batch gets `frameCursor = frameAnchor = committed`. (The apparent "separate pacing-cache fields" an earlier pass hypothesized at `UpdateQueueGlobals+0xc`/`+0x80` turned out to just be `curFrame` and `lastPanTimestamp` themselves, read under the mutex for cross-thread visibility — not distinct shadow fields.)
6. **After commit** — `build_overlap_dependency_list` runs again (dependencies now reflect final frame numbers), then per item: `advance_work_item_frames(item)` (§6.4), deep-copy (`update_item_copy`) onto a fresh node hooked onto `g_pListProcessedUpdates`, `processedUpdatesCount++`. The batch node is unhooked from `listIncomingUpdates`, its now-empty sub-list freed, and the `BatchNode` itself freed.
7. **Bottom-of-loop sweep [confirmed]** — every node still in `g_pListProcessedUpdates` with `phase < lutWidthMinus1` gets another `advance_work_item_frames` call.

**Exit:** both `g_pListProcessedUpdates` and `g_pListIncomingUpdates` empty *and* `shutdownRequested != 0` → thread exits.

### 6.2a `build_overlap_dependency_list` (0x3a838) — CONFIRMED

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

### 6.3 Display-side commit kernels — `FUN_0004f8f0` / `FUN_0004e680`

`(WorkItem* item, int32_t out_rect[4], int chunkIndex, int chunkCount)`, called from §6.2 step 4. **[confirmed]** chunking is by column, not row, despite the row-span gate above — `out_rect` is written `{rectY1, endCol, rectY0, startCol}`.

Per pixel (8-lane NEON): `new` = the byte from `item.gap`'s buffer (the same output `render_update_kernel` produces, widened to u16); `state` = `g_pStateBuffer[col][row]`; `is_sentinel = (new == 0x20)` (the same skip sentinel case 7 uses).

```
FUN_0004f8f0 (sync==0, "incremental"):
  if (is_sentinel || new==state) → unchanged, emit 0x0004, state/out_rect untouched
  else → state=new, emit (state<<5)|new, write state back, narrow out_rect to include this pixel

FUN_0004e680 (sync!=0, "force"):
  state = is_sentinel ? state : new
  unconditionally emit (state<<5)|state and write back — every pixel, out_rect always full
```

Both write their result into `item.sp3`'s buffer as well as `g_pStateBuffer`. `[guess]`: the `(state<<5)|value` packing is a plausible waveform-LUT index encoding; `sp3`'s downstream consumer wasn't traced.

### 6.4 Worker-side playback chain — deeper than it first looked

`advance_work_item_frames` (0x3a984) → `FUN_0003f294`/`FUN_0003f1f0` → `FUN_0003ec78` → `FUN_0004a140` or `FUN_0004a234`. Called from `display_thread_func`'s commit step and bottom-of-loop sweep (§6.2 steps 6/7) — not from the worker thread, which is why this chain stayed fully library even after §6.1 went native.

- **`advance_work_item_frames(WorkItem* item)` [confirmed]:**
  1. `frameCount = min(8 - phase%8, lutWidthMinus1 - phase)`.
  2. Computes 8 words `frame_buffer_addr((frameCursor+i) & 0xf)` for `i=0..7` — ring size is **16** frame slots (matches `FbInitParams.frameCount=0x10`), not 8 as an earlier pass assumed (8 is just how many slot addresses get precomputed per call).
  3. Branches on `item->intList` empty vs non-empty:
     - **Empty** (→ `FUN_0003f294`, "plain" kernel path): a backpressure gate against `nFrameCleanupCursor` further bounds `frameCount` if `phase` isn't 8-aligned; aborts (no kernel call) if the resulting budget is insufficient.
     - **Non-empty** (→ `FUN_0003f1f0`, "overlap-aware" kernel path): walks `intList` transitively (chases into each linked item's own dependencies, not a flat scan) folding a bound from `otherItem->frameCursor - 1` for every "settled" (its own intList empty) still-active linked item, plus a special-cased large bound if any linked item's lifetime exceeds this item's own `frameAnchor`. **[derived, needs A/B]** on the exact edge-case arithmetic — transcribe verbatim rather than simplifying if porting ahead of an A/B pass; only fires for items with overlap dependencies (not the common case).
  4. After the wrapper call, for every newly-advanced frame slot, marks dirty every byte in `[sp3.x0, sp3.x1]` of that slot's dirty-gate row (base `0x670d8 + slot*0x57c + x`, matching §6.2 step 1's array exactly) — the *producer* side of the stale-row cleanup this array feeds, and of `render_update_kernel` case 7's gate.
  5. Tail: if `frameCursor < curFrame - 1`, logs a "generator thread has fallen behind" warning (cosmetic). If `frameCursor > targetFrame`: `targetFrame = frameCursor`, then `pthread_cond_broadcast(&workerCond)` under `workerCondMutex` — this is the only place `targetFrame` is written, and how it reaches the (native) worker thread's wait loop.
- **`FUN_0003f294`** (item's `intList` empty) / **`FUN_0003f1f0`** (non-empty) — **[confirmed]** thin wrappers, signature `(void* frameSlots[8], WorkItem* item, int frameCount, int chunkIndex)` (chunkIndex always `0` from their only caller). Chunk count: default 1; if the rect is non-degenerate and `area = (x1-x0+1)*(y1-y0+1) > 20000`: `chunkCount = (x1-x0 < 10) ? 1 : 2`. Then `FUN_0003ec78(kernelFn, frameSlots, item, frameCount, chunkCount)` — the *only* difference between the two wrappers is which kernel pointer (`FUN_0004a140` vs `FUN_0004a234`) they pass.
- **`FUN_0003ec78(kernelFn, frameSlots, item, frameCount, chunkCount)` [confirmed]** — the third independent thread pool. `chunkCount < 2`: calls `kernelFn(frameSlots, item, frameCount, /*chunkIndex=*/0, /*chunkCount=*/1)` synchronously (chunkCount forced to 1 even if the caller passed something else). `chunkCount >= 2`: lazily spins up (once, process-lifetime) a `hardware_concurrency()`-sized pool, submits exactly `chunkCount` closures `{kernelFn, frameSlots, item, frameCount, chunkIndex=0..chunkCount-1, chunkCount}`, blocks until idle. This pool's own internal bookkeeping globals are pure implementation detail (nothing else in the binary reads them) — a native port doesn't need to replicate them 1:1, just "spin up N threads once, submit closures, wait," e.g. with `std::thread`/`std::mutex`/`std::condition_variable`. **Commit (runs once per call, after the synchronous call or after all chunks finish):**
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
- **`FUN_0004a234`** ("overlap-aware" kernel) — `[guess]`, shape only. Selected exactly when the item has overlap-dependency links. `frameCount` cases 1–3 delegate to three further, **entirely unreversed** functions: `FUN_0004a3f8`, `FUN_0004a9e0`, `FUN_0004b098`. Only fires for items overlapping other in-flight items — not the common case, and not exercised by the current emulator test suite.

### 6.5 Thread pool inventory

Three independent, lazily-spun-up, `hardware_concurrency()`-sized, process-lifetime thread pools exist in this binary — none shared with each other:

| Pool owner | Spun up from | Chunking rule | Serves |
|---|---|---|---|
| `dispatch_update_regions` | `swtcon_update`'s call into it | exactly 2, by row | `render_update_kernel` |
| `dispatch_processed_regions` (0x50660) | `display_thread_func` step 4 | 1 or 2 (merged-rect X-span<29 gate — corrected from an earlier "row-span" assumption), by column | `FUN_0004f8f0`/`FUN_0004e680` |
| `FUN_0003ec78` | `FUN_0003f294`/`FUN_0003f1f0` | area>20000px && width<10 → 1, else 2 | `FUN_0004a140`/`FUN_0004a234` |

---

## 7. Shutdown flow

Native (`native_close_fb`, `native_unlock_pid_file`, `native_free_LUT`,
`native_free_statebuffer`, etc.), draining both update-queue lists and
joining both threads (the native worker thread and the still-library display
thread - `pthread_join` doesn't care which) before releasing native buffers.
See `AGENTS.md` Phase 2 for the initial port and the exit-time-destructor
double-free fix (the library's own `destroy_waveform_struct` exit handler
means the native waveform struct must be an intentionally-leaked
`std::vector`, not owned/freed natively).

---

## 8. Open questions

Everything not marked `[confirmed]` above, plus:

- **`render_update_kernel`'s bitfield semantics** (§5.2) and **base-offset arithmetic**.
- **`FUN_0004a140`'s exact NEON shift constants** (§6.4) — shape confirmed, not term-verified.
- **The `(state<<5)|value` packing** (§6.3) and `sp3`'s downstream consumer.
- **`FUN_0004a234` and its three delegates** (`FUN_0004a3f8`/`FUN_0004a9e0`/`FUN_0004b098`) — fully unreversed; overlap-only, deferrable.
- **`dispatch_processed_regions`'s rectangle-merge algorithm** (§6.2 step 4) — new finding this pass, genuinely unreversed (not the simple bounding-box union an earlier pass assumed). Recommend keeping this call by-address rather than guessing at a native replacement.
- **The incoming-batch gate-check's `sync`/`fullRefresh` dependency filter** (§6.2 step 3) — fields confirmed, exact byte offsets in this inlined copy not pinned down.
- **`advance_work_item_frames`'s overlap-aware bound computation** (§6.4, the `FUN_0003f1f0` path) — shape confirmed, exact edge-case arithmetic not closed out; transcribe verbatim rather than simplifying if porting ahead of an A/B pass.
- **`FUN_00050660`'s completion-tracking globals** — pool shape confirmed, a few exact field semantics aren't (why they're left `DAT_`-named in Ghidra, §2). (`FUN_0003ec78`'s own pool globals, by contrast, are confirmed as pure implementation detail nothing else reads - safe to reimplement with ordinary `std::thread`/mutex/condvar without replicating them.)

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
| `worker_thread_func` (0x3ae38) | Phase 5 | Native (`native_display.cpp`) |
| `pan_to_frame` (0x53fxx, unconditional pan variant) | Phase 5 | Native (`native_pan_to_frame`) |
| `write_flash_prime_pattern` (0x53c04) | Phase 5 | Native (`native_write_lut_pattern`, shares `native_init_lut`'s fill body) |
| `reset_statebuffer_neutral` (0x4fbe0) | Phase 5 | Native (`native_reset_statebuffer_neutral`) |
| `read_lut_packed_pixel` (0x40c58) | Phase 5 | Native (`native_read_lut_packed_pixel`) |

Still library, update path:

| Function | Address | Status |
|---|---|---|
| `dispatch_update_regions` | 0x4fff8 | Control flow + output struct confirmed; drives `render_update_kernel` |
| `render_update_kernel` | 0x4e7b8 | Formulas derived, bitfield semantics unverified |

Still library, display pipeline (all reversed to control-flow-confirmed detail this pass unless noted):

| Function | Address | Role |
|---|---|---|
| `display_thread_func` | 0x3d2ac | The remaining persistent thread; see §6.2 |
| `build_overlap_dependency_list` | 0x3a838 | Builds/clears an item's overlap-dependency `intList` against `g_pListProcessedUpdates` - fully confirmed, see §6.2a |
| `advance_work_item_frames` | 0x3a984 | Advances one item by up to 8 (of 16 ring) frames; marks the backBuffer dirty gate - see §6.4 |
| `copy_init_frame_row` | 0x53be4 | Restores one stale frame-slot row from the LUT blob's fixed reference row - newly named this pass |
| `dispatch_processed_regions` | 0x50660 | Second independent dispatcher - hides a genuinely unreversed rectangle-merge algorithm, **not** a simple bounding-box union as an earlier pass assumed; recommend calling by address indefinitely rather than guessing at a port |
| `FUN_0003f294` / `FUN_0003f1f0` | 0x3f294 / 0x3f1f0 | Thin wrappers into `FUN_0003ec78` - confirmed, see §6.4 |
| `FUN_0003ec78` | 0x3ec78 | Third thread pool; commits item phase/frameCursor after the kernel runs - confirmed, see §6.4 |
| `FUN_0004a140` | 0x4a140 | "Plain" per-frame OR-accumulated waveform bit-plane kernel - call signature confirmed, body `[derived]` |
| `FUN_0004a234` | 0x4a234 | "Overlap-aware" kernel variant; delegates to 3 unreversed functions for frameCount 1–3 - call signature confirmed, body `[guess]` |
| `FUN_0004a3f8` / `FUN_0004a9e0` / `FUN_0004b098` | 0x4a3f8 / 0x4a9e0 / 0x4b098 | Unreversed; `FUN_0004a234`'s delegates |
| `FUN_0004f8f0` / `FUN_0004e680` | 0x4f8f0 / 0x4e680 | Display-side commit kernels (incremental vs. force) - call signature confirmed (no hidden args), body `[guess]`/`[derived]` |

Native (Phase 5, this pass) - see `native_display.cpp`:

| Function | Address | Role |
|---|---|---|
| `worker_thread_func` | 0x3ae38 | Panel-driving frame-pacing loop - see §6.1. Ported natively; no longer started by address |
| `write_flash_prime_pattern` | 0x53c04 | Shares `native_init_lut`'s fill body, parameterized by dest+pattern |
| `read_lut_packed_pixel` | 0x40c58 | Generic bit-unpacking read of one packed pixel from a LUTEntry |
| `reset_statebuffer_neutral` | 0x4fbe0 | Reapplies the `0x1e001e` statebuffer fill |

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
