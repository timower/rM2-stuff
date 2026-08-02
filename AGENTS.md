Be brief.
Only comment in code if they clarify, explain why not what.
Avoid globals when possible.
Use `clang-format` to format C++/C files.

# Project structure

 * `libs/` contains:
   * `swtcon` a software epaper 'TCON' for the reMarkable2 tablet.
   * `rm2fb` a epaper framebuffer multiplexer.
   * `rMlib` various utilities to draw to the framebuffer and build apps.
   * `unistdpp` C++ wrapper around Unix/Linux APIs.
 * `apps/`
   * `rocket` Launcher for apps on the tablet.
   * `yaft` terminal emulator
   * `tilem` TI-84+ emulator
 * `tools/` various testing utlities
   * `rm2fb-emu` Viewer & testing tool for the `rm2fb` based terminal.
   * `ioctl-dump` preload library to dump framebuffer related IOCTLs.

# Building and Testing Steps

### 1. Compile for Device

Build the project using `ninja` in the dev folder (no need for `nix` environment):
```bash
ninja -C build/dev
```

If you change a `CMakeLists.txt` (e.g. add a compile flag), ninja must reconfigure,
which needs the toolchain/pkg-config from the nix env. Reconfigure with:
```bash
PKG_CONFIG_PATH="" TOOLCHAIN_ROOT=/nix/store/1arv0dc51097f6g9kqhvlg74wrfwgybr-remarkable2-toolchain-5.0.58 cmake --preset dev
```

### 2. Compile for Host

Build the relevant simulated apps and testing binaries using:
```bash
ninja -C build/dev-host
```

### 2. Testing

Unit tests using ctest are defined in `test/unit`.
Full integration test using nix are in `nix/test`.

There is an emulator available at `RemEmu` over ssh, if it's not up you can
start it using:
```bash
nix build .#rm-emu
./result/bin/run_vm  -serial mon:stdio
```

Individual nix tests can be ran using:
```bash
 nix build .#checks.x86_64-linux.xochitl
```
They also have `golden` attributes to just generate the reference screenshots.

