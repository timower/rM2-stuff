# Swtcon Re-Implementation Plan & Status

Our goal is to sever the dependency on the black-box `libqsgepaper.so` library by fully reversing and re-implementing the Software Timing Controller (swtcon) logic natively in C++.

## Phase 1: Architectural Refactoring [COMPLETE]
- Created `swtcon.h` and `swtcon.cpp` to provide a clean abstraction.
- Moved `libqsgepaper.so` `dlsym` loading logic into the backend.
- Allowed `qsgepaper-test` and `main.cpp` to run obliviously of how the init/update/shutdown are executed.

## Phase 2: Re-implementing `swtcon_shutdown` [COMPLETE]
- Re-implemented termination logic (`is_fb_blanked`, `close_fb`, `unlock_pid_file`).
- Graceful joining of library threads and memory cleanup.

## Phase 3: `swtcon_init` Re-implementation [COMPLETE]
- Re-implemented native allocation and generation of the state buffers (`state_buf`, `image_buffer`).
- Re-implemented `native_init_framebuffer`.
- Re-implemented `native_init_lut` with correct hardware bit-swizzling and configuration values.
- Re-implemented `native_load_waveform` with full `.wbf` format parser, RLE decompression, and native struct generation.

## Phase 4: Re-implementing `swtcon_update` [WORKING END-TO-END]
Native init + all three update modes (HQ/medium/clearing) + shutdown now run
end-to-end on the emulator with a clean exit (EXIT=0). Fixes applied this round:
- **ABI (RESOLVED):** Earlier notes claimed the library used the old
  `_GLIBCXX_USE_CXX11_ABI=0` string ABI. Backwards. `qsgepaper_init` and
  `select_waveform_lut` (0x4535c) show the **new** SSO string ABI (`=1`):
  `ModeEntry` name is 24 bytes and `luts` sits at offset 0x18. Tool is pinned to
  `_GLIBCXX_USE_CXX11_ABI=1` in CMakeLists.
- **Buffer wiring (FIXED):** The library keeps four separate buffers; native init
  had allocated two and mis-wired them, leaving the gamma table `g_pGammaTable`
  @0x6d1d4 (128 x 0x88 = 0x4400) NULL → near-null fault in `render_update_kernel`
  (0x4e7b8). Now all four are allocated/wired: image @0x670bc (0xff), screen
  @0x670c0 (calloc 0x281ac0), statebuffer @0x6d1d0 (0x1e), gamma @0x6d1d4.
- **LUT packing (FIXED):** `native_load_waveform`'s bit-packing reset the dest
  column index each source row instead of running 0..iVar27^2-1. Matches
  `load_waveform` @0x458e8.
- **`.wbf` RLE decode (FIXED — this was the mode-3 crash):** the run-length
  decode advanced the read index by 1 for a run, but `wbf_decode_waveform`
  (0x54560) advances by 2 (value byte + length byte), so the length byte was
  re-read as data — corrupting every mode's LUT. Also the mode loop was `<
  mode_count` but the library is inclusive (`<= mode_count`, so `DU4` was
  dropped). After fixing both, the native waveform struct is **byte-for-byte
  identical** to the library's (verified via `swtcon_dump_waveform` A/B: same 8
  modes, LUT sizes, and FNV checksums). Mode 3 no longer crashes.
- **fb_var_screeninfo (FIXED):** `native_init_framebuffer` now writes a real
  `struct fb_var_screeninfo`; fields match the library `init_framebuffer`
  (0x53c0c) exactly (`Put info` verified). `g_nFbSizeX` is now one full frame in
  bytes (bpp*xres*yres/8), not the line length — `frame_buffer_addr` (0x53fd0)
  depends on it.
- **Exit double-free (FIXED):** the library registers an exit-time destructor
  `destroy_waveform_struct` (0x451b0, via `_INIT_3`) for `g_waveform_struct`. Our
  handle shared the same array via the `memcpy`, so both freed it. The native
  handle is now an intentionally-leaked heap `std::vector` so only the library
  destructor frees the contents.

### Tooling added
- SIGSEGV handler in `main.cpp` prints faulting PCs as libqsgepaper Ghidra
  addresses (`pc - runtime_offset`). Invaluable for locating faults in the
  black-box library.
- `swtcon_dump_waveform()` walks `g_waveform_struct` with the native
  `ModeEntry`/`LUTEntry` types (layout matches, ABI=1) and prints per-LUT
  metadata + FNV checksum. Works for both native- and library-populated structs,
  enabling byte-level A/B. To A/B: flip the `#if 1`/`#if 0` in `swtcon_init`.

### Init tables must be byte-identical to the library (FIXED)
On real hardware the pan succeeded but the screen still rendered wrong, because
the render kernels (library code) gather through fixed tables that native init
built incorrectly. Verified by dumping+`cmp`-ing each table native-vs-library
(`swtcon_dump_buffers`, gated behind `SWTCON_DUMP=1`). All now match byte-for-byte:
- **Gamma table** (`init_statebuffer` @0x4fad4): the source values are UNSIGNED
  16-bit and the library **pre-increments** the pointer (first read at Ghidra
  0x596da, skipping 0x596d8). Native used a signed `int16_t` cast and started one
  index early. Fixed by (a) regenerating `statebuffer_table.h` from file offset
  0x496da (17407 `ushort`s) so index maps directly, and (b) dropping the signed
  cast.
- **LUT** (`init_LUT`/`FUN_00053ac4`/`FUN_00053a30`): several wrong constants —
  the vector-immediate decompilation was misleading; read them from the
  disassembly instead. `vmov.i32 #0x430000`==0x00430000 (was 0x43000000),
  `#0x410000`==0x00410000 (was 0x41000000), `vorr #0x20000` (was 0x20000000), the
  `0x100000` loop ORs 0x100000 into BOTH words (was 0x100004 on one). Two loop
  bounds were also off-by-one (inclusive of 0x8e / 0x66 — the store happens before
  the exit compare). The middle `native_init_lut_sub` call takes -1, not 0 (r1 is
  left at 0xffffffff from the first call).
- **Statebuffer** (`init_statebuffer`): filled with the 32-bit pattern
  `0x001e001e` (per-pixel uint16 state = 0x001e), NOT `memset(0x1e)` which makes
  each state 0x1e1e. This directly corrupts the transitions the render kernels
  compute.

With these, native init's LUT, gamma, statebuffer, AND waveform are all
byte-identical to the library's on the device. **Needs re-test on hardware** to
confirm the screen now renders correctly.

### Display frame streaming / `FBIOPAN` (FIXED)
The worker (`worker_thread_func` 0x3ae38) streams sub-frames via
`pan_to_frame(g_nCurFrame & 0xf)` (0x53fec), which rewrites only `yoffset` in the
**global** `g_fbVarScreeninfo` (0x6d3a0) and re-issues `FBIOPAN_DISPLAY`.
`native_init_framebuffer` had filled a *local* `fb_var_screeninfo` and never
populated that global, so the pan saw `yres=0`/`xres=0`: on hardware the driver
rejected it ("Pan failed" x hundreds, screen never refreshed); on the emulator
the mock didn't validate so it logged `FBIOPAN: 0 0` (yoffset = frame*yres =
frame*0). Fix: `native_init_framebuffer` now fills the module globals
`g_fbVarScreeninfoNative`/`g_fbFixScreeninfoNative`, and `swtcon_init` memcpys
them into the library globals `g_fbVarScreeninfo` (0x6d3a0, 0xa0 bytes) and
`g_fbFixScreeninfo` (0x6d35c, 0x44 bytes). Emulator now steps `FBIOPAN` 0..16.
Needs re-test on real hardware to confirm the panel refreshes.

## Phase 5: Re-implementing the Display Threads [TODO]
- The threads currently run the *library's* `worker_thread_func` (0x3ae38) and
  `display_thread_func` (0x3d2ac) by address. Reverse and reimplement them
  natively (this also subsumes the FBIOPAN streaming bug above).

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
