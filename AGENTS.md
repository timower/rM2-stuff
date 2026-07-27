# Swtcon Re-Implementation Plan & Status

Our goal is to sever the dependency on the black-box `libqsgepaper.so` library by fully reversing and re-implementing the Software Timing Controller (swtcon) logic natively in C++.

**The goal is full independence — no exceptions by default.** Every function
still called into the library by address (`resolve_ptr<...(*)...>(0x...)`) is
a dependency that needs to eventually go. Sizing something "large" just means
"port last, after everything smaller" — never "skip." As long as any
function is still called by address, the whole library stays `dlopen`'d and
mapped, so nothing is actually exempt.

**Update:** as of this pass, there are zero remaining by-address *function*
calls anywhere in the production pipeline (see Phase 5/6 and the playback-
kernel entry below) — the last one, `FUN_0004a234`, is gone. This is NOT the
same as "the library can be unloaded," though: this codebase's native code
still treats large swaths of the library's own `.bss`/`.data` segments as its
own runtime global storage via `resolve_ptr<T*>(addr)` *data* accesses (the
update-queue state, framebuffer/statebuffer globals, mutexes, the dirty-gate
array, a vtable pointer, etc. — grep `resolve_ptr<` without `(*)` for the
full list). Replacing all of that with genuinely own-allocated storage is a
separate, much larger undertaking than porting functions and is not
in scope for what "reversing the functions" tracked here means; treat it as
a distinct future phase if full independence (no `dlopen` at all) is ever
wanted.

See `swtcon_architecture.md` for a standing reference on *how the system
works* (data formats, control flow, algorithms, native and still-library
parts together, with confidence tags on anything not byte-verified). This
file tracks phase-by-phase porting *status* instead — what's done, what's
left, and what to do next.

## Progress

- **Phase 1 — Architectural refactoring:** COMPLETE. `swtcon.h`/`swtcon.cpp` abstraction in place; `main.cpp` and `qsgepaper-test` run oblivious to how init/update/shutdown are actually executed.
- **Phase 2 — `swtcon_shutdown`:** COMPLETE. Native termination logic, graceful thread join, memory cleanup.
- **Phase 3 — `swtcon_init`:** COMPLETE. Native allocation/generation of all init-time state (statebuffer, gamma table, LUT, waveform loading, framebuffer, temperature sensor). **Confirmed on real hardware:** screen renders and panel refreshes correctly.
- **Phase 4 — `swtcon_update` control flow:** COMPLETE. `swtcon_lock/update/unlock_post/wait` fully native. **Confirmed on hardware:** all three test update modes (HQ/medium/clearing) render correctly.
- **Phase 4b — `swtcon_update` leaf routines:** COMPLETE. All leaves (`update_item_ctor`, `clamp_update_rect`, `get_current_temperature`, `update_item_copy`, `free_update_region_list`, `subtract_update_region`, `build_update_batch`, `select_waveform_lut`, `update_lut_is_valid`) are native, including the small anonymous LUTEntry+shared_ptr allocator at 0x408a8 (`native_make_empty_lut`) that `update_item_ctor`/`select_waveform_lut`'s fallback both used to call by address — found missing from this file's "remaining still-library calls" tally after it had already been marked complete elsewhere, a reminder to grep for `resolve_ptr<...(*)...>` call sites directly rather than trust a prose summary. Ported with its own small native vtable (dispose/destroy) rather than reusing the library's — that allocator's vtable pointer is computed through a more ambiguous GOT-indirect addressing pattern than `dispatch_update_regions`'s RegionRows block, not worth risking a silent mismatch with `release_sp`'s `vt[2]`/`vt[3]` convention. **Confirmed on the emulator** with clean `EXIT=0` across HQ/medium/clearing plus the overlap-update test suite (hardware re-confirmation pending).
- **Phase 5 — Display threads (`worker_thread_func`, `display_thread_func`):** COMPLETE. Both persistent threads are native (`native_display.cpp`): `native_worker_thread_func` (the panel-driving frame-pacing loop) and `native_display_thread_func` (the `WorkItem`/dependency-list state machine — GC, stale-row cleanup, incoming-batch intake/gate-check/dispatch/commit, and the worker-side playback chain through `advance_work_item_frames`/`FUN_0003f294`/`FUN_0003f1f0`/`FUN_0003ec78`). Re-derived directly from disassembly rather than the earlier higher-level sketch, which corrected two things along the way: the incoming-batch gate-check's `sync`/`fullRefresh` filter is now exact (was `[derived, not fully closed]`), and `advance_work_item_frames`'s kernel-selection rule was actually backwards from what the doc guessed (see `swtcon_architecture.md` §6.4). **Confirmed on the emulator** with clean `EXIT=0` across HQ/medium/clearing plus the full overlap-update test suite, continuous `FBIOPAN` traffic throughout, clean shutdown (hardware re-confirmation still pending). `FUN_0004a140`/`FUN_0004a234` (the worker-side playback kernels) were by-address and unreversed at the time; both are native now (see below).
- **`dispatch_processed_regions` — SOLVED, native, and WIRED IN (confirmed on the emulator).** What looked like a genuine cross-item rectangle-merge algorithm (an earlier framing) turned out, on empirical investigation via a dedicated tool (`tools/qsgepaper-preload/dispatch_processed_regions_probe.cpp` — calls the real library function directly with controlled synthetic `WorkItem` batches and reads back what actually happened, same technique as `render_kernel_addr_map.cpp`), to be **per-item-only bookkeeping with zero cross-item interaction** — 7 passing experiments including the definitive one (two independent items in one batch, dispatched together; the unrelated item's rect came back completely unaffected). Also nailed down `FUN_0004f8f0`/`FUN_0004e680`'s exact per-pixel packing formula: the packed transition value is `(oldState<<5)|newValue` and the "unchanged" sentinel is `0x0400`. See `swtcon_architecture.md` §6.2 step 4 / §6.3. The native port (`native_dispatch_processed_regions_native`/`native_commit_item` in `native_display.cpp`) is wired in at the real call site. **The "integration hazard" that previously kept it by-address — a deterministic SIGSEGV in the still-library `FUN_0004a234` on the first HQ update — was a stale `WorkItem.stateDataPtr` (+0x44), not `FUN_0004a234` itself.** The real `dispatch_processed_regions` caches `sp3.ptr->dataPtr` into `item+0x44` right after allocating `sp3`, and both playback kernels (`FUN_0004a140`/`FUN_0004a234`) read their per-pixel state through that cached pointer, not via `sp3.ptr->dataPtr`; `native_commit_item` now sets it. That's exactly why the earlier elimination log (not downstream state, not `sp3` content, not timing, not global init) came back all-negative — none of those touched +0x44. **Confirmed on the emulator** with the native path live: clean `EXIT=0` across HQ/medium/clearing plus all five overlap tests, continuous `FBIOPAN`, clean shutdown. **Confirmed on hardware too**, after one follow-up fix: +0x44 is not the sp3 buffer *base* — it's that base **rebased to the item's post-narrowing rect origin** (`sp3.dataPtr + stride*(rectX0-sp3.x0) + (rectY0-sp3.y0)`, the same rebasing as `gap` vs `regionRows`). The incremental commit narrows `item.rect` inward from the 8-aligned seed the sp3 buffer was sized for, and the playback kernels index +0x44 with narrowed-rect-relative coords. Seeding it to the base (offset 0) is correct only for the force path and unsplit items; for any narrowed/split item it read state one column + 8 rows off → **edge artifacts** (white seams where an update overdrew a previous one), visible only on real e-ink since the digital state/framebuffer settle identically. Found via a native-vs-library A/B harness (`SWTCON_LIBDISPATCH` env toggle, still in `native_display_thread_func`) that diffs the *transient* waveform frames — the library's +0x44 came back as `sp3.dataPtr + 0x670` on a narrowed item, exactly `stride*(804-803)+(1072-1064)`. `native_commit_item` now rebases it after narrowing.
- **Phase 6 — `render_update_kernel` / `dispatch_update_regions`:** COMPLETE. Both the per-pixel formulas and the address/180°-rotation arithmetic are **confirmed** and natively ported (`native_dispatch_update_regions`/`native_render_update_kernel` in `native_update.cpp`). The formulas were re-derived from raw ARM disassembly and runtime A/B-checked (`tools/qsgepaper-preload/render_kernel_verify.cpp`) via a multiset trick that doesn't need addressing solved first; the addressing itself was then solved empirically (`tools/qsgepaper-preload/render_kernel_addr_map.cpp`) via binary search per output byte — no manual decoding of the chunked/NEON/rotated pointer arithmetic was needed. See `swtcon_architecture.md` §5.1/§5.2. `dispatch_update_regions`'s two-way thread-pool chunking is provably invisible in the output (it only splits the same computation into two disjoint column ranges), so the native port is a single straightforward pass with no threading. **Confirmed on the emulator** with clean `EXIT=0` across HQ/medium/clearing plus the full overlap-update test suite (hardware re-confirmation still pending).

`swtcon_update`'s entire path is now fully native — zero remaining by-address library calls in it. In the display pipeline (Phase 5), `dispatch_processed_regions` (0x50660) and its two display-commit kernels (`FUN_0004f8f0`/`FUN_0004e680`) are now native and wired in (see above), and so are both worker-side playback kernels, `FUN_0004a140` and `FUN_0004a234` (see below). **Zero remaining by-address function calls anywhere in the production pipeline** — the only still-library by-address call left in the whole codebase is `native_dispatch_processed_regions` in `native_display.cpp`, which is kept *deliberately* as a `SWTCON_LIBDISPATCH`-gated A/B testing fallback, not a production dependency.

- **`FUN_0004a140` ("plain" playback kernel) — SOLVED, native, and WIRED IN (confirmed on the emulator).** Reversed via `tools/qsgepaper-preload/playback_kernel_probe.cpp`, the same "dlopen the real library, call the real function directly with a controlled synthetic `WorkItem`, read back what happened" technique as `render_kernel_addr_map.cpp`/`dispatch_processed_regions_probe.cpp`. Every buffer the probe hands the kernel (state buffer, LUT data, the `frameSlots[8]` pointer array itself, even the `WorkItem`'s own containing node) is backed by an `mmap` `PROT_NONE` guard page on both sides — a first pass with plain `malloc`'d buffers found the kernel call "returns without crashing" but silently corrupts memory, discovered only much later at an unrelated `free()`; guard pages turn that into an immediate, localizing `SIGSEGV`. (The actual first corruption culprit turned out to be a bug in the probe itself, not the kernel: `LUTEntry`'s destructor unconditionally `free()`s `.data`, and `TestItem` embedded a `LUTEntry` by value, so it fired on every guard-paged LUT buffer at scope exit — nulling `.data` before that fixed it. Worth remembering next time a "clean return, corruption discovered later" pattern shows up: check for an implicit destructor on a raw pointer field before suspecting the library call.) When black-box probing alone stalled (zero output from a plausible-looking call), reading `FUN_0004a140`'s Ghidra decompile directly (via the `mcp__ghidra__decompile_function` MCP tool, `libqsgepaper_3.23.0.54.so`) resolved it immediately rather than more guessing. Fully confirmed and ported (`native_playback_kernel_plain` in `native_display.cpp`):
  - The LUT-index formula matches the already-native `native_read_lut_packed_pixel` exactly: the raw `stateDataPtr` (+0x44) u16 per pixel is used directly as `mw*row+col` (a flat multiply-add, *not* a `>>5`/`&0x1f` split — only numerically identical to the `(oldState<<5)|newState` packing confirmed elsewhere in this codebase because `mode_width==32` in that case).
  - The destination address formula: `frameSlots[k] + ((col + 3)*0x104 + (rectY0>>3) + group + 0x1a) * 4` (`col` absolute, `group` the 0-based 8-row-group index within the rect, `k` the output-frame index) — byte-verified against five targeted probe experiments (single group, multi-group at `base+group*4`, and multi-column chunking).
  - Y is block-quantized by 8 (`((rectY1-rectY0)+1)>>3` gates the whole write loop — a literal 1×1 test rect silently no-ops here, which is why the probe's first few runs produced zero output with no error); X is not block-quantized.
  - Drive values are 2-bit; within one 8-row group, row `r` (0-based from the group's start) OR-accumulates into bit position `(7-r)*2` of a 16-bit word — row 0 at the *top*, row 7 at the *bottom* (confirmed via 8 isolated single-row probe experiments), matching this codebase's established 180°-rotation convention rather than a naive lane order.
  - `frameCount` (1–8) does **not** mean N independent LUT lookups: the kernel computes an 8-entry shared buffer once per column/row-group (one entry per possible sub-phase 0–7 packed into the current LUT word) and `frameSlots[k]` just reads out shared-buffer entry `(phase&7)+k` — confirmed both by decompiling case 2 and by a probe experiment planting two different values at adjacent sub-phases of the same LUT word and observing them land in `frameSlots[0]`/`frameSlots[1]` respectively. This is exactly why `native_advance_work_item_frames` always picks `frameCount` so `phase&7+frameCount` never crosses an 8-sub-phase LUT-word boundary — the native port needs no case-by-case logic at all, just one parametrized loop.
  - `FUN_0004a140`'s prologue jump-tables on `frameCount` 0–8; case 0 falls through to a *second*, Ghidra-unrecoverable nested jump table (`Could not recover jumptable at 0x47be8`) — irrelevant here since `native_advance_work_item_frames` never calls the plain-kernel path with `frameCount==0` (guarded by its own early-return).
  - The function calls no allocation/free, touches no global (`DAT_*`) addresses, spawns no threads, and never writes to `*item` — every `item` field it touches (`0xc`/`0x10`/`0x14`/`0x18`/`0x28`/`0x2c`/`0x3c`/`0x44`) is read-only, and mode_width/bit_depth aside, the 2-bit/8-row packing constants are hardcoded immediates in the real kernel too (matches `native_load_waveform` always producing `bit_depth=2` LUTs).
  - **Confirmed on the emulator** via the full `swtcon-ab-test` A/B pipeline (not just the isolated probe): `KERN` log coverage shows the real test matrix exercises the plain kernel exclusively at `frameCount=8`; a native-vs-library `--compare` run matched byte-for-byte everywhere except `seq=26-29`, which is the pre-existing documented real-time-jitter window (case 8/9's `usleep`-based submissions) — confirmed noise, not a regression, since a native-vs-native rerun shows the identical mismatch pattern with the library entirely out of the loop.
- **`FUN_0004a234` ("overlap-aware" playback kernel) — SOLVED, native, and WIRED IN (confirmed on the emulator).** Turned out **not to be a distinct algorithm at all.** Rather than reversing it independently, a decisive test (`playback_kernel_probe.cpp` Experiment 7) calls both `FUN_0004a140` and the real `FUN_0004a234` on an *identical* synthetic `WorkItem`/state/LUT (a rich, pseudo-random, non-uniform fill spanning 4 columns × 16 rows, so a coincidental match is implausible) for every `frameCount` 1–8 — **including the three cases (1, 2, 3) that `FUN_0004a234` itself tail-calls out to separate, still fully unreversed delegates** (`FUN_0004a3f8`/`FUN_0004a9e0`/`FUN_0004b098`) **for** — and diffs every byte of the output: all eight cases came back byte-for-byte identical. A Ghidra decompile of case 8 (94/117 real overlap-kernel calls in one `ab-test` run, vs. 23 for case 5 and 0 for every other case — checked via the `KERN` log before investing in a decompile) independently corroborates this: it touches the *exact same* `WorkItem` fields as `FUN_0004a140` (`0xc`/`0x10`/`0x14`/`0x18`/`0x28`/`0x2c`/`0x3c`/`0x44`), the same LUT-index formula, the same destination address formula, and the same row→bit packing order — no new field, no global, no `intList` access, no allocation, no threads. The only difference is NEON unrolling strategy: cases 4–8 extract all 8 sub-phases from one lane-shifted vector at once (valid only because the overlap-kernel selection rule guarantees `phase&7==0` whenever this path is taken, so there's no `(phase&7)+k` offset to compute, unlike `FUN_0004a140`'s general case). **Net result: `native_playback_kernel_plain` now backs *both* `native_dispatch_plain_kernel` and `native_dispatch_overlap_kernel`** (`native_display.cpp`) — one native function, zero remaining by-address playback-kernel calls, and the three delegates never need reversing at all. **Confirmed on the emulator**: the full `swtcon-ab-test` A/B pipeline's `STATE` records (the settled, final pixel-level output per test step) match the library **perfectly, zero mismatches, across all 9 test cases** — the only remaining `--compare` divergences are transient `PLAY` per-frame-capture timing jitter (inherently real-time-dependent, the same documented class of noise as `seq=26-29`'s case 8/9 window, now also touching `seq=10` purely because native code runs at different wall-clock speed than the by-address call did, shifting which real-time frame tick a capture lands on — confirmed non-causal by reproducing the identical `seq=10` mismatch in a native-vs-native rerun with the library entirely out of the loop) and the pre-existing, already-documented `seq=28/29` `DISP` noise. `qsgepaper-test`'s full interactive suite (including the two playback-coverage scenarios) also runs clean, `EXIT=0`, on the emulator.
  - **Follow-up ("there must be a reason they're separate functions"):** decompiling `FUN_0004a3f8` (case 1's delegate) directly answers this — the code really is meaningfully different, not a trivial duplicate. It does alignment-based column-chunking (`-(((uint)ptr & 0x3f) >> 4) & 3`-style leading/trailing-remainder splitting) and an 8×4 transpose/prefetch bulk loop, structurally unlike `FUN_0004a140`'s simple one-column-at-a-time loop — genuine hand-tuned NEON engineering, most likely per-`(frameCount, phase-alignment)` performance specialization (`FUN_0004a140` is the fully general case, needing the `(phase&7)+k` slice for arbitrary alignment; `FUN_0004a234`'s cases 4-8 and the three delegates are all fast paths only valid at `phase&7==0`, some further specialized for wide-column bulk throughput). Critically, it still touches only the *exact same* `WorkItem` fields as everything else here — no `intList`, no new global, no allocation — so "why a separate function" has an engineering answer (performance), not a semantic one (no code path here has any way to observe or react to *another* item's overlap state; that's entirely the caller's `advance_work_item_frames` selection-rule's job, see `swtcon_architecture.md` §6.4). Re-ran the identical-input diff specifically designed to stress the parts that differ - a 128-column rect (wide enough to exercise the bulk transpose path, not just the tail/remainder handling a narrow rect would trivially hit either way) and explicit `chunkCount=2` splitting (exercising `FUN_0004a3f8`'s own independently-computed chunk boundary, not just `FUN_0004a140`/`FUN_0004a234`'s shared one) - still byte-for-byte identical across every case. Xrefs confirm neither kernel is called from anywhere but its own wrapper (`FUN_0003f294`/`FUN_0003f1f0`), so there's no other call site hinting at a different intended use either.

## Next steps

1. **A/B-verify the remaining derived formulas** before trusting them enough to ship native code: the display thread's frame-pacing target formula (precisely reversed, see `swtcon_architecture.md` §6.2 step 5 — but still worth an A/B pass before depending on it). `FUN_0004f8f0`/`FUN_0004e680`'s packing formula no longer needs this — it's confirmed (see above). Same technique as `render_kernel_verify.cpp`/`render_kernel_addr_map.cpp` — write a native candidate, diff its output byte-for-byte (or via an addressing-independent invariant) against the library on identical input.
2. Apply the same Ghidra rename pass already done for other native functions to the remaining confirmed-but-unnamed ones (`FUN_0004f8f0`/`FUN_0004e680`/`FUN_0003ec78`/`FUN_0003f294`/`FUN_0003f1f0`/`FUN_0004a140`). `FUN_0004a234` and its three delegates never got (and now never need) a native port of their own, so there's nothing to rename there — `native_playback_kernel_plain` already carries a real name.
3. **Hardware re-confirmation** — Phases 4b, 5, and 6 plus the now-wired-in `dispatch_processed_regions`/`FUN_0004a140`/`FUN_0004a234`'s emulator-only confirmations should be re-run on real hardware when next convenient.
4. **(Optional, separate scope) Replace the library's `.bss` as native global storage** — see the "Update" note at the top of this file. Not "reversing a function," but the actual remaining blocker to ever dropping the `dlopen` entirely.

---

# Building and Testing Steps

When testing on the local emulator (`RemEmu`), the physical `/dev/fb0` device does not exist. We use the `libioctl-dump.so` mockup library to intercept and handle `/dev/fb0` operations.

### 1. Compile
Build the project using `ninja` in the dev folder (no need for `nix` environment):
```bash
ninja -C build/dev
```

If you change a `CMakeLists.txt` (e.g. add a compile flag), ninja must reconfigure,
which needs the toolchain/pkg-config from the nix env. Reconfigure with:
```bash
PKG_CONFIG_PATH="" TOOLCHAIN_ROOT=/nix/store/1arv0dc51097f6g9kqhvlg74wrfwgybr-remarkable2-toolchain-5.0.58 cmake --preset dev
```

Note: `qsgepaper-test` is built with `_GLIBCXX_USE_CXX11_ABI=1` (the modern
default) so that our native `std::string`/`std::vector`/`std::list` layouts match
those in `libqsgepaper.so`. The library uses the **new** SSO string ABI: in
`qsgepaper_init` each `ModeEntry` name string is 0x18 (24) bytes, and
`FUN_0004535c` reads `ModeEntry::luts` at offset 0x18 — which only lines up with a
24-byte (new-ABI) `std::string`.

### 2. Upload to Emulator
Transfer both the test executable and the mocking library to the emulator:
```bash
scp build/dev/tools/qsgepaper-preload/qsgepaper-test build/dev/tools/ioctl-dump/libioctl-dump.so RemEmu:/home/root/
```

### 3. Run Test
Execute the test on the emulator using `LD_PRELOAD` to inject the framebuffer mock:
```bash
ssh RemEmu 'LD_PRELOAD=/home/root/libioctl-dump.so /home/root/qsgepaper-test'
```
Note that a successful run requires some input, see the `getchar()` calls.

### 4. A/B regression harness (`swtcon-ab-test`)

`swtcon-ab-test` (`ab_harness.cpp` + `ab_capture.cpp`) is the automated
differential test. It runs a fixed matrix of update sequences (overlap splits,
non-8-aligned rects, cross-batch overdraw - the cases that make an item's sp3
rebase offset nonzero) through the *real* pipeline and, when
`SWTCON_AB_CAPTURE=<path>` is set, records a sorted, pointer-normalized dump of:

- `DISP` - per surviving item after dispatch: narrowed rect, sp3 bounds, sp3
  content hash, and `stateDataPtr` as a u16 **offset** from `sp3.dataPtr`
  (`sdpoff`, the field the +0x44 bug got wrong);
- `PLAY` - each transient waveform frame, captured race-free on the display
  thread right after the playback kernel writes it (keyed by `seqId`/frame/slot);
- `STATE` - the settled state-buffer hash per step (the framebuffer ring is
  deliberately not hashed - the live worker thread races it).

The capture hooks live on the production path (`native_display.cpp`) and are
no-ops unless the env var is set, so this exercises exactly what ships. Run the
same input once native and once through the still-library dispatch
(`SWTCON_LIBDISPATCH=1`, the ground truth), then diff:

```bash
scp build/dev/tools/qsgepaper-preload/swtcon-ab-test RemEmu:/home/root/
ssh RemEmu 'cd /home/root && \
  SWTCON_AB_CAPTURE=/tmp/native.txt                     LD_PRELOAD=./libioctl-dump.so ./swtcon-ab-test && \
  SWTCON_AB_CAPTURE=/tmp/lib.txt SWTCON_LIBDISPATCH=1    LD_PRELOAD=./libioctl-dump.so ./swtcon-ab-test && \
  ./swtcon-ab-test --compare /tmp/native.txt /tmp/lib.txt'
```

`--compare` exits 0 on a full match, 1 on any divergence (printing the
localizing records), 2 on I/O error - so it drops straight into CI. Native runs
are deterministic, so two native captures also compare equal; that doubles as a
race check. Validated by reverting the +0x44 rebase: the harness fails and
points straight at the mismatched `sdpoff`/`PLAY` records.
