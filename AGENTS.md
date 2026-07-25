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

## Phase 4: Re-implementing `swtcon_update` [IN PROGRESS]
- **Current Issue:** The implementation currently segfaults when `swtcon_update` accesses the natively-generated `g_waveform_struct`. This is because `ModeEntry` contains a `std::string`, and its size/layout in the modern C++ ABI (`_GLIBCXX_USE_CXX11_ABI=1`) differs from the older ABI used in `libqsgepaper.so` (`_GLIBCXX_USE_CXX11_ABI=0`), misaligning the fields!
- **Next Steps:** Complete the C++ ABI layout fix for `ModeEntry`, then finish translating `FUN_0003ccac` which takes an `update_data` struct and queues it onto the global `g_pListIncomingUpdates` native `std::list`.

## Phase 5: Re-implementing the Display Threads [TODO]
- Decompile `LAB_0003d2ac` (the main display queue thread).
- Build our own thread loops that pop items off our native `std::list`, apply the correct waveform logic, and flush bits to `/dev/fb0`.

---

# Building and Testing Steps

When testing on the local emulator (`RemEmu`), the physical `/dev/fb0` device does not exist. We use the `libioctl-dump.so` mockup library to intercept and handle `/dev/fb0` operations.

### 1. Compile
Build the project using `ninja` in the dev folder (no need for `nix` environment):
```bash
ninja -C build/dev
```

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
