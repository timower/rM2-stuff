# Swtcon Re-Implementation Plan & Status

Our goal is to sever the dependency on the black-box `libqsgepaper.so` library by fully reversing and re-implementing the Software Timing Controller (swtcon) logic natively in C++.

**The goal is full independence — no exceptions by default.** Every function
still called into the library by address (`resolve_ptr<...>(0x...)`) is a
dependency that needs to eventually go. Sizing something "large" just means
"port last, after everything smaller" — never "skip." As long as any
function is still called by address, the whole library stays `dlopen`'d and
mapped, so nothing is actually exempt.

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
- **Phase 5 — Display threads (`worker_thread_func`, `display_thread_func`):** COMPLETE. Both persistent threads are native (`native_display.cpp`): `native_worker_thread_func` (the panel-driving frame-pacing loop) and `native_display_thread_func` (the `WorkItem`/dependency-list state machine — GC, stale-row cleanup, incoming-batch intake/gate-check/dispatch/commit, and the worker-side playback chain through `advance_work_item_frames`/`FUN_0003f294`/`FUN_0003f1f0`/`FUN_0003ec78`). Re-derived directly from disassembly rather than the earlier higher-level sketch, which corrected two things along the way: the incoming-batch gate-check's `sync`/`fullRefresh` filter is now exact (was `[derived, not fully closed]`), and `advance_work_item_frames`'s kernel-selection rule was actually backwards from what the doc guessed (see `swtcon_architecture.md` §6.4). **Confirmed on the emulator** with clean `EXIT=0` across HQ/medium/clearing plus the full overlap-update test suite, continuous `FBIOPAN` traffic throughout, clean shutdown (hardware re-confirmation still pending). `FUN_0004a140`/`FUN_0004a234` (the worker-side playback kernels) remain by-address, unreversed.
- **`dispatch_processed_regions` — SOLVED, native, and WIRED IN (confirmed on the emulator).** What looked like a genuine cross-item rectangle-merge algorithm (an earlier framing) turned out, on empirical investigation via a dedicated tool (`tools/qsgepaper-preload/dispatch_processed_regions_probe.cpp` — calls the real library function directly with controlled synthetic `WorkItem` batches and reads back what actually happened, same technique as `render_kernel_addr_map.cpp`), to be **per-item-only bookkeeping with zero cross-item interaction** — 7 passing experiments including the definitive one (two independent items in one batch, dispatched together; the unrelated item's rect came back completely unaffected). Also nailed down `FUN_0004f8f0`/`FUN_0004e680`'s exact per-pixel packing formula: the packed transition value is `(oldState<<5)|newValue` and the "unchanged" sentinel is `0x0400`. See `swtcon_architecture.md` §6.2 step 4 / §6.3. The native port (`native_dispatch_processed_regions_native`/`native_commit_item` in `native_display.cpp`) is wired in at the real call site. **The "integration hazard" that previously kept it by-address — a deterministic SIGSEGV in the still-library `FUN_0004a234` on the first HQ update — was a stale `WorkItem.stateDataPtr` (+0x44), not `FUN_0004a234` itself.** The real `dispatch_processed_regions` caches `sp3.ptr->dataPtr` into `item+0x44` right after allocating `sp3`, and both playback kernels (`FUN_0004a140`/`FUN_0004a234`) read their per-pixel state through that cached pointer, not via `sp3.ptr->dataPtr`; `native_commit_item` now sets it. That's exactly why the earlier elimination log (not downstream state, not `sp3` content, not timing, not global init) came back all-negative — none of those touched +0x44. **Confirmed on the emulator** with the native path live: clean `EXIT=0` across HQ/medium/clearing plus all five overlap tests, continuous `FBIOPAN`, clean shutdown. **Confirmed on hardware too**, after one follow-up fix: +0x44 is not the sp3 buffer *base* — it's that base **rebased to the item's post-narrowing rect origin** (`sp3.dataPtr + stride*(rectX0-sp3.x0) + (rectY0-sp3.y0)`, the same rebasing as `gap` vs `regionRows`). The incremental commit narrows `item.rect` inward from the 8-aligned seed the sp3 buffer was sized for, and the playback kernels index +0x44 with narrowed-rect-relative coords. Seeding it to the base (offset 0) is correct only for the force path and unsplit items; for any narrowed/split item it read state one column + 8 rows off → **edge artifacts** (white seams where an update overdrew a previous one), visible only on real e-ink since the digital state/framebuffer settle identically. Found via a native-vs-library A/B harness (`SWTCON_LIBDISPATCH` env toggle, still in `native_display_thread_func`) that diffs the *transient* waveform frames — the library's +0x44 came back as `sp3.dataPtr + 0x670` on a narrowed item, exactly `stride*(804-803)+(1072-1064)`. `native_commit_item` now rebases it after narrowing.
- **Phase 6 — `render_update_kernel` / `dispatch_update_regions`:** COMPLETE. Both the per-pixel formulas and the address/180°-rotation arithmetic are **confirmed** and natively ported (`native_dispatch_update_regions`/`native_render_update_kernel` in `native_update.cpp`). The formulas were re-derived from raw ARM disassembly and runtime A/B-checked (`tools/qsgepaper-preload/render_kernel_verify.cpp`) via a multiset trick that doesn't need addressing solved first; the addressing itself was then solved empirically (`tools/qsgepaper-preload/render_kernel_addr_map.cpp`) via binary search per output byte — no manual decoding of the chunked/NEON/rotated pointer arithmetic was needed. See `swtcon_architecture.md` §5.1/§5.2. `dispatch_update_regions`'s two-way thread-pool chunking is provably invisible in the output (it only splits the same computation into two disjoint column ranges), so the native port is a single straightforward pass with no threading. **Confirmed on the emulator** with clean `EXIT=0` across HQ/medium/clearing plus the full overlap-update test suite (hardware re-confirmation still pending).

`swtcon_update`'s entire path is now fully native — zero remaining by-address library calls in it. In the display pipeline (Phase 5), `dispatch_processed_regions` (0x50660) and its two display-commit kernels (`FUN_0004f8f0`/`FUN_0004e680`) are now native and wired in (see above); `FUN_0004a140`/`FUN_0004a234` (the worker-side playback kernels) remain genuinely unreversed and by-address — the only remaining by-address calls in the whole pipeline.

- **`FUN_0004a140` ("plain" playback kernel) — IN PROGRESS, isolation-probe technique confirmed viable.** Built `tools/qsgepaper-preload/playback_kernel_probe.cpp`, the same "dlopen the real library, call the real function directly with a controlled synthetic `WorkItem`, read back what happened" technique as `render_kernel_addr_map.cpp`/`dispatch_processed_regions_probe.cpp`. Every buffer the probe hands the kernel (state buffer, LUT data, the `frameSlots[8]` pointer array itself, even the `WorkItem`'s own containing node) is backed by an `mmap` `PROT_NONE` guard page on both sides — a first pass with plain `malloc`'d buffers found the kernel call "returns without crashing" but silently corrupts memory, discovered only much later at an unrelated `free()`; guard pages turn that into an immediate, localizing `SIGSEGV`. (The actual first corruption culprit turned out to be a bug in the probe itself, not the kernel: `LUTEntry`'s destructor unconditionally `free()`s `.data`, and `TestItem` embedded a `LUTEntry` by value, so it fired on every guard-paged LUT buffer at scope exit — nulling `.data` before that fixed it. Worth remembering next time a "clean return, corruption discovered later" pattern shows up: check for an implicit destructor on a raw pointer field before suspecting the library call.) When black-box probing alone stalled (zero output from a plausible-looking call), reading `FUN_0004a140`'s Ghidra decompile directly (via the `mcp__ghidra__decompile_function` MCP tool, `libqsgepaper_3.23.0.54.so`) resolved it immediately rather than more guessing — confirmed:
  - The LUT-index formula matches the already-native `native_read_lut_packed_pixel` exactly: the raw `stateDataPtr` (+0x44) u16 per pixel is used directly as `mw*row+col` (a flat multiply-add, *not* a `>>5`/`&0x1f` split — only numerically identical to the `(oldState<<5)|newState` packing confirmed elsewhere in this codebase because `mode_width==32` in that case).
  - The destination address formula for `frameCount==1`: `frameSlots[0] + ((col + rectX0 + 3)*0x104 + (rectY0>>3) + 0x1a) * 4` — byte-verified against a live call (predicted `0x7fbd8`, observed `0x7fbd8`).
  - Y is block-quantized by 8 (`((rectY1-rectY0)+1)>>3` gates the whole write loop — a literal 1×1 test rect silently no-ops here, which is why the probe's first few runs produced zero output with no error); X is not block-quantized.
  - Drive values are 2-bit, OR-packed 8-to-a-word (confirmed round-trip: uniform transition → LUT value 3 over an 8-row column produced `0xff 0xff`).
  - **Still open:** the exact bit-position each of the 8 packed rows lands in within that 16-bit pair — a mixed-transition test (only row 0 active) landed in the second byte's high bits, not matching the naive shift-order read off the decompile's NEON lane sequence yet. This is the immediate next thing to pin down before writing `native_playback_kernel_plain`.
  - `FUN_0004a140`'s prologue jump-tables on `frameCount` 0–8; case 0 falls through to a *second*, Ghidra-unrecoverable nested jump table (`Could not recover jumptable at 0x47be8`) that will need manual disassembly if a real call ever hits `frameCount==0` — cases 1–8 (the only ones normal playback advances produce) don't touch it.
  - The function calls no allocation/free, touches no global (`DAT_*`) addresses, spawns no threads, and never writes to `*item` — every `item` field it touches (`0xc`/`0x10`/`0x14`/`0x18`/`0x28`/`0x2c`/`0x3c`/`0x44`) is read-only. So once the bit-order is closed out, the native port is a pure function of `(frameSlots, item's known fields)` with no hidden state to replicate.

## Next steps

1. **Close out `FUN_0004a140`**: pin down the per-row bit-position mapping within the packed 16-bit output word (see above), extend the probe/formula to `frameCount` cases 2–8, then write `native_playback_kernel_plain` in `native_display.cpp` and A/B-verify it byte-for-byte against the library before wiring it in.
2. **Reverse `FUN_0004a234`** (0x4a234, the "overlap-aware" variant) — a many-KB function with its own 9-way jump table on `frameCount` and three further unreversed delegates (`FUN_0004a3f8`/`FUN_0004a9e0`/`FUN_0004b098`) for cases 1–3. Apply the same two-pronged technique that unblocked `FUN_0004a140`: black-box calls through `playback_kernel_probe.cpp` first, and read the Ghidra decompile directly (`mcp__ghidra__decompile_function` on `libqsgepaper_3.23.0.54.so`) as soon as guessing stalls rather than continuing to iterate blind. It's reached whenever `intList` has no active dependency and `phase` is 8-aligned (see `swtcon_architecture.md` §6.4) — not just the very first HQ update as an earlier pass assumed. `ab_capture_kernel`'s `KERN` log (wired into both `native_dispatch_plain_kernel`/`native_dispatch_overlap_kernel` in `native_display.cpp`) records every real dispatch's `(kernel, frameCount, chunkCount)` so the emulator test matrix's actual coverage of this kernel's cases is known rather than assumed — `swtcon-ab-test`'s cases 8/9 and `qsgepaper-test`'s matching interactive scenarios were added specifically to exercise the previously-uncovered "plain"-kernel-via-active-dependency and `chunk_count==1` paths; check that log's coverage before trusting any case as tested.
3. **A/B-verify the remaining derived formulas** before trusting them enough to ship native code: the display thread's frame-pacing target formula (precisely reversed, see `swtcon_architecture.md` §6.2 step 5 — but still worth an A/B pass before depending on it). `FUN_0004f8f0`/`FUN_0004e680`'s packing formula no longer needs this — it's confirmed (see above). Same technique as `render_kernel_verify.cpp`/`render_kernel_addr_map.cpp` — write a native candidate, diff its output byte-for-byte (or via an addressing-independent invariant) against the library on identical input.
4. Apply the same Ghidra rename pass already done for other native functions to the remaining confirmed-but-unnamed ones (`FUN_0004f8f0`/`FUN_0004e680`/`FUN_0003ec78`/`FUN_0003f294`/`FUN_0003f1f0`); hold off on `FUN_0004a140`/`FUN_0004a234` and `FUN_0004a234`'s three delegates until they're actually fully ported.
5. **Hardware re-confirmation** — Phases 4b, 5, and 6 plus the now-wired-in `dispatch_processed_regions`' emulator-only confirmations should be re-run on real hardware when next convenient.

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
