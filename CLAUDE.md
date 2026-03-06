# rM2-stuff — reMarkable 2 Firmware 3.25 Support

Fork of [timower/rM2-stuff](https://github.com/timower/rM2-stuff) with firmware 3.25.1.1 support.
Upstream PR: https://github.com/timower/rM2-stuff/pull/50

## Device Access

```sh
ssh root@10.11.99.1          # USB connection (password on Settings > General > Help)
ssh root@192.168.1.168        # WiFi connection (fallback if USB IP doesn't work)
```

If the USB IP (10.11.99.1) doesn't connect, use the WiFi IP (192.168.1.168) instead.
The deploy/scp commands below use 10.11.99.1 but substitute 192.168.1.168 as needed.

The device runs BusyBox — many GNU flags don't work:
- `od -A n` → use `od -x`
- `head -1` → use `head -n 1`
- No `pgrep -f`, no `script`, no `file`

## Terminal (yaft)

```sh
ssh root@10.11.99.1
terminal                      # launch yaft on e-ink screen
terminal stop                 # force return to reMarkable UI
nohup terminal &              # survives SSH disconnect
```

The `terminal` script at `/opt/bin/terminal` stops xochitl, starts rm2fb, runs yaft,
and restores xochitl on exit.

## Type Folio Keyboard

The Type Folio has no physical `[` or `]` keys. The pogo driver's default keymap
already maps CapsLock to Escape — no hwdb remap needed.

**WARNING:** Do NOT use udev hwdb `KEYBOARD_KEY_xx` entries for the Type Folio.
The Type Folio is a matrix keyboard (bus=0019, vendor=2EDD, product=0001) and
hwdb scancodes are matrix indices, not AT scancodes. Using AT scancode `3a`
(CapsLock on normal keyboards) actually remaps KEY_4's matrix position, breaking
the 4 key. The hwdb remap was removed after discovering this.

Other useful keys: Fn + `-` = `=`, Fn + 9 = `[`, Fn + 0 = `]`.

## SSH from Device

The device uses Dropbear, not OpenSSH:
```sh
dropbearkey -t ed25519 -f ~/.ssh/id_dropbear     # generate key
dropbearkey -y -f ~/.ssh/id_dropbear              # show public key
TERM=xterm dbclient -T -i ~/.ssh/id_dropbear user@host /bin/bash  # connect
```

## Build and Deploy

### Build (cross-compile on macOS)
```sh
cd ~/repos/LbdInternal/rm2-stuff
nix build .#dev-cross
```

### Deploy
```sh
# Prepare binaries
cp result/lib/librm2fb_server.so /tmp/librm2fb_server.so
cp result/lib/librm2fb_client.so.1.1.0 /tmp/librm2fb_client.so
cp result/bin/rm2fb_server /tmp/rm2fb_server_exe
chmod +w /tmp/librm2fb_server.so /tmp/librm2fb_client.so /tmp/rm2fb_server_exe

nix shell nixpkgs#patchelf -c bash -c '
  patchelf --set-interpreter /lib/ld-linux-armhf.so.3 --set-rpath /opt/lib /tmp/rm2fb_server_exe
  patchelf --set-rpath /opt/lib /tmp/librm2fb_server.so
  patchelf --set-rpath /opt/lib /tmp/librm2fb_client.so
'

scp /tmp/rm2fb_server_exe root@10.11.99.1:/opt/bin/rm2fb_server
scp /tmp/librm2fb_server.so root@10.11.99.1:/opt/lib/librm2fb_server.so
scp /tmp/librm2fb_client.so root@10.11.99.1:/opt/lib/librm2fb_client.so.1.1.0
```

**CRITICAL**: The Version3.25 code runs in `librm2fb_server.so` (loaded via
LD_PRELOAD into xochitl), NOT the rm2fb_server executable. Always deploy the library.

### Restart after deploy
```sh
ssh root@10.11.99.1 'systemctl stop rm2fb; systemctl reset-failed rm2fb; systemctl start rm2fb'
```

If the service crash-loops: `systemctl reset-failed rm2fb` before restarting.

### Quick Build-Deploy-Test Cycle
```sh
nix build .#dev-cross && \
cp result/lib/librm2fb_server.so /tmp/librm2fb_server.so && \
chmod +w /tmp/librm2fb_server.so && \
nix shell nixpkgs#patchelf -c patchelf --set-rpath /opt/lib /tmp/librm2fb_server.so && \
scp /tmp/librm2fb_server.so root@10.11.99.1:/opt/lib/librm2fb_server.so && \
ssh root@10.11.99.1 'systemctl stop rm2fb; systemctl reset-failed rm2fb; systemctl start rm2fb'
```

### Screenshot (verify display output)
```sh
./scripts/screenshot_rm2.sh                         # saves to /tmp/rm2_screenshot.png, opens it
./scripts/screenshot_rm2.sh out.png root@10.11.99.1 # custom output and host
```

## Viewing the Screen Remotely

Cannot read display output from /dev/fb0 — SWTCON doesn't write there.
Instead, verify display updates via:

```sh
# LCDIF interrupt count (increases ~50-90 per e-ink refresh)
ssh root@10.11.99.1 'cat /proc/interrupts | grep lcdif'

# Non-white pixels in shared framebuffer (0 = nothing displayed)
ssh root@10.11.99.1 'dd if=/dev/shm/swtfb.01 bs=4096 count=1280 2>/dev/null | od -x | grep -cv "ffff.*ffff.*ffff.*ffff.*ffff.*ffff.*ffff.*ffff"'

# Server logs
ssh root@10.11.99.1 'journalctl -u rm2fb --no-pager -n 30'
```

## Debugging

```sh
# Check what's running
ssh root@10.11.99.1 'ps | grep -E "xochitl|yaft|rm2fb" | grep -v grep'

# rm2fb runs as xochitl with LD_PRELOAD — look for:
#   {xochitl} /opt/bin/rm2fb_server     <- rm2fb mode
#   /usr/bin/xochitl --system           <- stock mode

# Read process memory (e.g. swtconData state)
ssh root@10.11.99.1 'PID=$(ps | grep "{xochitl}" | grep -v grep | awk "{print \$1}"); cat /proc/$PID/maps | head -n 20'

# SWTCON hardware status
ssh root@10.11.99.1 'cat /sys/class/hwmon/hwmon1/temp0'
```

## Architecture

Firmware 3.25 moved EPFramebufferSwtcon from libqsgepaper.so into xochitl
(statically linked). The rm2fb server must use LD_PRELOAD mode.

Display update pipeline:
```
yaft -> RGB565 shared FB -> rm2fb server converts RGB565→ARGB32 (+ Y8 gray)
     -> actualUpdate(0x57e6f0) creates UpdateMsgs from ARGB32
     -> processAndSignal(0x57d5e4) wakes framegen
     -> SWTCON -> LCDIF -> e-ink panel
```

### Buffer Requirements (actualUpdate)

actualUpdate reads from the **ARGB32 buffer** for rendering. The gray buffer is used
for dirty detection (current vs previous frame comparison). Key findings:

- **ARGB32 is required** — actualUpdate does not render without it
- **Gray buffer write can be skipped** when the client pre-populates it (GrayReady flag)
- actualUpdate does NOT modify the gray buffer (confirmed by before/after comparison)
- The GrayReady fast path (flag `0x8` in UpdateParams) skips the server's RGB565→Y8
  conversion (~30% of the conversion loop work)

### Canvas Rotation Caveat

`getGrayCanvas()` returns an **unrotated** canvas (direct linear addressing), while
drawing sub-canvases may have rotation applied via `Canvas::getPtr()`. In landscape
mode, the same logical pixel maps to **different linear indices** in RGB565 vs Y8.
This means Y8→ARGB32 expansion (using Y8 values to populate ARGB32) fails in
landscape — the pixel positions don't correspond. The current approach writes Y8 at
unrotated positions for the gray buffer and uses RGB565→ARGB32 for the ARGB buffer.

### Key Addresses (xochitl 3.25.1.1)
```
Build ID:          4dec15723de1c4ee431fd09079fc218d95cbe2b3
instance():        0x583020
actualUpdate:      0x57e6f0
processAndSignal:  0x57d5e4
shutdown:          0x7438F8
singleton ptr:     0x01324090
swtconData ptr:    0x013240E4
queue mutex:       swtconData + 0x54
futex word:        swtconData + 0x30
```

### Finding Addresses for Future Firmware

1. `scp root@10.11.99.1:/usr/bin/xochitl /tmp/xochitl`
2. `readelf -n /tmp/xochitl` — get build ID
3. Search for "SWTCON initialized" string -> trace to `EPFramebuffer::instance()`
4. Find Fusion vtable -> vtable[23] has the update dispatcher
5. actualUpdate is called after locking mutex; processAndSignal is called after
6. Use Ghidra or Python + capstone for disassembly:
```python
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
with open('/tmp/xochitl', 'rb') as f:
    f.seek(addr - 0x10000)  # xochitl loads at 0x10000
    code = f.read(0x200)
for insn in cs.disasm(code, addr):
    print(f"  0x{insn.address:08x}: {insn.mnemonic} {insn.op_str}")
```

## Files Modified from Upstream

| File | Change |
|------|--------|
| `libs/rm2fb/Versions/Version3.25.cpp` | New — full 3.25 implementation with GrayReady fast path |
| `libs/rm2fb/Versions/Version.cpp` | 3.25 build ID + ELF PT_NOTE parsing |
| `libs/rm2fb/Versions/Version.h` | Declared `version_3_25_0` |
| `libs/rm2fb/PreloadHooks.h` | Added `Mmap` hook |
| `libs/rm2fb/CMakeLists.txt` | Added Version3.25.cpp |
| `libs/rm2fb/Message.h` | Added `gray_prepopulated_flag = 0x8` to UpdateParams |
| `libs/rMlib/include/FrameBuffer.h` | Added `GrayReady` flag, `getGrayCanvas()`, `canvasOffset()` |
| `libs/rMlib/FrameBuffer.cpp` | Expanded mmap for Y8 gray buffer, `getGrayCanvas()` impl |
| `apps/yaft/screen.cpp` | GrayReady flags, Y8 gray writes, cell-level dirty tracking |
| `apps/yaft/screen.h` | Added `grayCanvas` member, updated `drawLine` signature |
| `apps/yaft/YaftWidget.cpp` | 32ms debounce timer for batching terminal output |
| `apps/yaft/YaftWidget.h` | Added `drawTimer`/`drawPending` members |

### Performance: E-ink Refresh Batching

The biggest performance lever is **minimizing panel refreshes** (each costs 50-250ms),
not optimizing pixel conversion (1-5ms). yaft uses a 32ms debounce timer: terminal
output is parsed immediately but `setState()` (triggering redraw + panel refresh) is
deferred until 32ms of silence. This batches rapid output (e.g., scrolling, `ls`)
into ~30 fps max instead of one refresh per pty read.

### Allocation Sizes
- Gray buffer: `1404 * 1872 * 1 = 0x281ac0` (stable across versions)
- ARGB buffer: `1404 * 1872 * 4 = 0xa06b00` (new in 3.25)
- /dev/fb0 mmap: `0x17BD800` (may vary)

## Device State

- Firmware: 3.25.1.1
- Auto-updates disabled (fakeupdateengine installed)
- libevdev symlink: `/usr/lib/libevdev.so.2 -> /opt/lib/libevdev.so.2`
- SSH key: `/home/root/.ssh/id_dropbear` (ed25519)
- Helper script: `/opt/bin/terminal`
- README on device: `/opt/README-rm2fb-3.25.md`

---

## Reverse Engineering Reference

### The Problem

rm2fb v0.1.3 doesn't support firmware 3.25. It crashes because:

1. **Build IDs not recognized** — xochitl and libqsgepaper.so have new build IDs
2. **EPFramebufferSwtcon moved** — statically linked into xochitl (was in libqsgepaper.so)
3. The server's `dlopen(libqsgepaper.so)` path finds only `EPFramebufferDesktop` (a software renderer), not the hardware SWTCON path

### How EPFramebufferSwtcon Was Found in xochitl

RTTI strings confirmed the classes exist in xochitl (not libqsgepaper):

```
strings /tmp/xochitl | grep -i swtcon
-> 19EPFramebufferSwtcon
-> 19EPFramebufferFusion
-> SWTCON initialized \o/
```

### How Addresses Were Discovered

1. **EPFramebuffer::instance() (0x583020):** Found via cross-references to
   `operator new(140)` and the "SWTCON initialized" string.

2. **actualUpdate (0x57e6f0):** Found via Fusion vtable[23] disassembly.
   Takes `(void* this, QRegion& region, int waveform, int mode, int flags)`.
   Ignores `this`; uses global swtconData pointer at 0x013240E4.

3. **processAndSignal (0x57d5e4):** Called immediately after actualUpdate in
   vtable[23]. No args — accesses globals directly. Sequence: copy msgs from
   +0x58BC queue -> +0x48 processed list, reset queue, unlock mutex at +0x54,
   atomic increment futex at +0x30, futex_wake.

4. **Fusion vtable[23] (0x57fffc):** The update entry point. Creates two QRegions
   via intersection/subtraction, calls actualUpdate twice (mode=7 for changed,
   mode=12 for unchanged). We call actualUpdate once with mode=7 only.

5. **Build ID:** Extracted via `readelf -n /tmp/xochitl` or `xxd -s 0x170 -l 20 -i`.

### Buffer Redirections

During EPFramebuffer::instance() initialization, malloc/calloc hooks intercept:

| Allocation | Size | Purpose |
|-----------|------|---------|
| `0x281ac0` (2,628,288) | 1404x1872x1 | Gray buffer -> redirected to shared memory |
| `0xa06b00` (10,515,200) | 1404x1872x4 | ARGB32 buffer -> pointer captured |
| `0x17BD800` (24,893,440) | /dev/fb0 mmap | Framebuffer -> pointer captured |

The gray buffer redirect allows clients to pre-populate Y8 data (GrayReady flag).
actualUpdate primarily reads the **ARGB32 buffer** for rendering; the gray buffer is
used for dirty detection (comparing current vs previous frame).

### Update Mechanism

The update sequence mirrors Fusion vtable[23], but simplified:

1. Lock queue mutex at `swtconData + 0x54`
2. Call `actualUpdate(instance, QRegion, waveform=2, mode=7, flags=1)` — creates UpdateMsgs
3. Call `processAndSignal()` — moves msgs to processed list, unlocks mutex, increments futex, wakes framegen

**Key insight:** Only mode=7 ("changed region") is used. The original vtable[23] also
calls mode=12 ("unchanged complement"), but using the same QRegion for both causes
mode=12 to override mode=7, telling framegen nothing changed.

### Version.cpp: ELF Build ID Parsing

The original code hardcoded the build-id address at `0x10180`. In 3.25, the linker
placed `.note.gnu.build-id` at a different offset. The fix parses PT_NOTE program
headers from the in-memory ELF to find the GNU build-id note dynamically, with
fallback to the legacy address for older firmware.

### Failed Approaches

1. **triggerUpdate (vtable[24], 0x56d978):** Sets pending flag + signals condvar.
   Internal update thread wakes but finds no dirty regions -> no UpdateMsgs created.

2. **Dual actualUpdate (mode=7 + mode=12):** Using the same QRegion for both
   causes mode=12 to override, telling framegen nothing changed.

3. **Checking /dev/fb0 for output:** SWTCON does NOT write to the /dev/fb0 mmap.
   The pan buffer content is stale from boot. LCDIF interrupt count is the correct
   way to verify display activity.

4. **Deploying only rm2fb_server executable:** The Version3.25 code runs in
   librm2fb_server.so (loaded via LD_PRELOAD), not the executable. Spent time
   debugging why changes had no effect before discovering the library wasn't updated.

### Tools Used

- **Ghidra** for initial xochitl disassembly
- **Python + capstone** for targeted ARM disassembly from macOS
- **Nix flake** for armv7l cross-compilation on aarch64-darwin
- **patchelf** for fixing interpreter/rpath on nix-built binaries

### Install Helper Script (for reference)

```sh
ssh root@10.11.99.1 'cat > /opt/bin/terminal << "SCRIPT"
#!/bin/sh
case "$1" in
  stop)
    systemctl stop rm2fb
    systemctl start xochitl
    echo "Switched to reMarkable UI"
    ;;
  *)
    systemctl stop xochitl 2>/dev/null
    systemctl start rm2fb
    sleep 2
    LD_PRELOAD=/opt/lib/librm2fb_client.so.1.1.0 /opt/bin/yaft
    systemctl stop rm2fb
    systemctl start xochitl
    echo "Switched to reMarkable UI"
    ;;
esac
SCRIPT
chmod +x /opt/bin/terminal'
```
