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
- **Phase 4b — `swtcon_update` leaf routines:** COMPLETE except the two noted below. All other leaves (`update_item_ctor`, `clamp_update_rect`, `get_current_temperature`, `update_item_copy`, `free_update_region_list`, `subtract_update_region`, `build_update_batch`, `select_waveform_lut`, `update_lut_is_valid`) are native. **Confirmed on the emulator** (hardware re-confirmation pending for the two most recent leaves) with clean `EXIT=0` across HQ/medium/clearing plus the overlap-update test suite.
- **Phase 5 — Display threads (`worker_thread_func`, `display_thread_func`):** COMPLETE. Both persistent threads are native (`native_display.cpp`): `native_worker_thread_func` (the panel-driving frame-pacing loop) and `native_display_thread_func` (the `WorkItem`/dependency-list state machine — GC, stale-row cleanup, incoming-batch intake/gate-check/dispatch/commit, and the worker-side playback chain through `advance_work_item_frames`/`FUN_0003f294`/`FUN_0003f1f0`/`FUN_0003ec78`). Re-derived directly from disassembly rather than the earlier higher-level sketch, which corrected two things along the way: the incoming-batch gate-check's `sync`/`fullRefresh` filter is now exact (was `[derived, not fully closed]`), and `advance_work_item_frames`'s kernel-selection rule was actually backwards from what the doc guessed (see `swtcon_architecture.md` §6.4). **Confirmed on the emulator** with clean `EXIT=0` across HQ/medium/clearing plus the full overlap-update test suite, continuous `FBIOPAN` traffic throughout, clean shutdown (hardware re-confirmation still pending). `dispatch_processed_regions` (0x50660) and its two playback kernels (0x4a140/0x4a234) stay still-library by-address calls — see `swtcon_architecture.md` §6.2 step 4 / §6.4 for why.
- **Phase 6 — `render_update_kernel` / `dispatch_update_regions`:** TODO, reversed and now formula-verified. Mode dispatch table and per-pixel formulas for all cases are **confirmed** — both re-derived from raw ARM disassembly and runtime A/B-checked against the real library function (`tools/qsgepaper-preload/render_kernel_verify.cpp`, see `swtcon_architecture.md` §5.2's verification box) — via a multiset trick that validates the formulas without needing the addressing/rotation logic solved first. The dispatcher's control flow and output struct are confirmed too. Only the per-pixel address/180°-rotation arithmetic remains unverified; that's the actual porting work left. The only two library calls left in the `swtcon_update` path itself.

Only remaining still-library calls anywhere in the native port: `dispatch_update_regions` (0x4fff8) + `render_update_kernel` (0x4e7b8) in the update path (Phase 6), and `dispatch_processed_regions` (0x50660) + its two playback kernels (0x4a140/0x4a234) in the display pipeline (Phase 5, deliberately deferred — see `swtcon_architecture.md` §6.2 step 4 / §8).

## Next steps

1. **Port `dispatch_update_regions`/`render_update_kernel`** (Phase 6) — now that the per-pixel formulas are confirmed (see above), the only unsolved piece is the per-pixel address/180°-rotation arithmetic (chunked iteration order → `dataBuffer`/`backBuffer`/output-buffer addresses). This is the only remaining update-path leaf and the largest remaining native-porting chunk in the whole project. In progress.
2. **A/B-verify the remaining derived formulas** before trusting them enough to ship native code: `FUN_0004a140`'s NEON shift constants, the display-commit kernels' `(state<<5)|value` packing, and the display thread's frame-pacing target formula (precisely reversed, see `swtcon_architecture.md` §6.2 step 5 — but still worth an A/B pass before depending on it). Same technique as `render_kernel_verify.cpp` — write a native candidate, diff its output byte-for-byte (or via an addressing-independent invariant, as that tool does) against the library on identical input.
3. **`dispatch_processed_regions`'s rectangle-merge algorithm** (0x50660, display pipeline) — decompilation shows a genuine interval/rectangle-merge algorithm (dynamic vector growth, node-based rect merging), not a simple bounding-box union. Keep calling it by address until that algorithm is actually understood — don't guess at a replacement.
4. **Decision point:** `FUN_0004a234`'s three delegate functions (`FUN_0004a3f8`/`FUN_0004a9e0`/`FUN_0004b098`, part of the worker-side playback kernel chain) are the only fully-unreversed functions left in the display pipeline. Per the corrected kernel-selection rule (`swtcon_architecture.md` §6.4), they're reachable whenever `advance_work_item_frames` picks the overlap-aware kernel (an 8-aligned phase with no active dependency left) with `frameCount` 1–3 — not exercised by the current emulator test suite — so still reasonable to defer.
5. Apply the same Ghidra rename pass already done for other native functions to the remaining confirmed-but-unnamed ones (`FUN_0004f8f0`/`FUN_0004e680`/`FUN_0003ec78`/`FUN_0004a140`/`FUN_0003f294`/`FUN_0003f1f0`); hold off on `FUN_0004a234` and its three delegates until they're actually reversed.

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
