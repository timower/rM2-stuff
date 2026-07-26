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
- **Phase 5 — Display threads (`worker_thread_func`, `display_thread_func`):** TODO, but fully mapped. Both threads' state machines, all three of the binary's independent thread pools, and both blit-kernel chains (display-side and worker-side) are reversed and documented in `swtcon_architecture.md` §6 — nothing here is native yet.
- **Phase 6 — `render_update_kernel` / `dispatch_update_regions`:** TODO, reversed. Mode dispatch table and per-pixel formulas for all cases are derived (not yet byte-verified); the dispatcher's control flow and output struct are confirmed. Still the only two library calls left in the `swtcon_update` path itself.

Only remaining still-library calls in the `swtcon_update` path: `dispatch_update_regions` (0x4fff8) and `render_update_kernel` (0x4e7b8). The display pipeline (Phase 5) is separately still 100% library.

## Next steps

1. **Decision point:** `FUN_0004a234`'s three delegate functions (`FUN_0004a3f8`/`FUN_0004a9e0`/`FUN_0004b098`, part of the worker-side playback kernel chain) are the only fully-unreversed functions left. They only fire for items overlapping other in-flight updates — not the common case, and not exercised by the current emulator test suite — so they're reasonable to defer rather than block on.
2. **A/B-verify the derived formulas** before trusting them enough to ship native code: `render_update_kernel`'s per-pixel bitfield semantics, `FUN_0004a140`'s NEON shift constants, the display-commit kernels' `(state<<5)|value` packing, and the display thread's frame-pacing target formula. Same technique already used for the gamma/LUT/statebuffer tables — write a native candidate, diff its output byte-for-byte against the library on identical input, let mismatches point at the wrong guess.
3. **Port Phase 5's confirmed control flow natively**: both thread state machines, all three dispatcher shells, `advance_work_item_frames`, `build_overlap_dependency_list`, and `FUN_0003ec78`'s phase/cursor commit logic have no open questions blocking a native port.
4. Once ported, port `dispatch_update_regions`/`render_update_kernel` (Phase 6) — deferred to last since it's the largest remaining chunk, not because it's exempt.
5. Apply the same Ghidra rename pass already done for the Phase 5 functions found so far to the remaining confirmed ones (`FUN_0004f8f0`/`FUN_0004e680`/`FUN_0003ec78`/`FUN_0004a140`/`FUN_0003f294`/`FUN_0003f1f0`); hold off on `FUN_0004a234` and its three delegates until they're actually reversed.

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
