# reMarkable 2 Terminal (Firmware 3.25)

rm2fb + yaft on reMarkable 2 firmware 3.25.1.1, enabling a terminal emulator
with Type Folio keyboard support for SSH access.

## Quick Start

SSH into your reMarkable (`ssh root@10.11.99.1`), then:

```sh
terminal          # launch yaft terminal (auto-returns to UI on exit)
terminal stop     # force-return to reMarkable UI if stuck
```

Inside yaft:

```sh
# SSH to another machine
TERM=xterm dbclient -T -i ~/.ssh/id_dropbear user@host /bin/bash

# Generate SSH key (first time only)
dropbearkey -t ed25519 -f ~/.ssh/id_dropbear
dropbearkey -y -f ~/.ssh/id_dropbear   # show public key

# Exit terminal (returns to reMarkable UI)
exit
```

### Type Folio Keyboard

- **Fn + key** for the third symbol (e.g., Fn + `-` = `=`)
- Standard modifier keys: Shift, Ctrl, Alt

## Architecture

### The Problem

rm2fb v0.1.3 doesn't support firmware 3.25. It crashes because:

1. **Build IDs not recognized** — xochitl and libqsgepaper.so have new build IDs
2. **EPFramebufferSwtcon moved** — statically linked into xochitl (was in libqsgepaper.so)
3. The server's `dlopen(libqsgepaper.so)` path finds only `EPFramebufferDesktop` (a software renderer), not the hardware SWTCON path

### The Solution

The rm2fb server runs in **LD_PRELOAD mode** (ServerLib.cpp), injected into xochitl's
process via `LD_PRELOAD=librm2fb_server.so`. This gives direct access to xochitl's
statically-linked SWTCON functions at their known addresses.

The display update pipeline:

```
yaft → RGB565 to shared FB (/dev/shm/swtfb.01)
     → rm2fb server converts RGB565 → Y8 gray + ARGB32
     → actualUpdate(0x57e6f0) creates UpdateMsgs
     → processAndSignal(0x57d5e4) queues msgs, wakes framegen
     → framegen thread → SWTCON voltages → LCDIF → e-ink panel
```

### Key Addresses (xochitl 3.25.1.1)

```
Build ID:           4d ec 15 72 3d e1 c4 ee 43 1f d0 90 79 fc 21 8d 95 cb e2 b3
EPFramebuffer::instance():  0x583020
actualUpdate:               0x57e6f0
processAndSignal:           0x57d5e4
Shutdown:                   0x7438F8
Singleton pointer:          0x01324090
hasShutdown flag:           0x0132AD00
swtconData pointer:         0x013240E4
Queue mutex offset:         swtconData + 0x54
Futex word offset:          swtconData + 0x30
```

### Buffer Redirections

During EPFramebuffer::instance() initialization, malloc/calloc hooks intercept:

| Allocation | Size | Purpose |
|-----------|------|---------|
| `0x281ac0` (2,628,288) | 1404×1872×1 | Gray buffer → redirected to shared memory |
| `0xa06b00` (10,515,200) | 1404×1872×4 | ARGB32 buffer → pointer captured |
| `0x17BD800` (24,893,440) | /dev/fb0 mmap | Framebuffer → pointer captured |

The gray buffer redirect is the critical one — actualUpdate reads pixel data from it
when building UpdateMsgs for the framegen thread.

### Update Mechanism

The update sequence mirrors Fusion vtable[23], but simplified:

1. Lock queue mutex at `swtconData + 0x54`
2. Call `actualUpdate(instance, QRegion, waveform=2, mode=7, flags=1)` — creates UpdateMsgs
3. Call `processAndSignal()` — moves msgs to processed list, unlocks mutex, increments futex, wakes framegen

**Key insight:** Only mode=7 ("changed region") is used. The original vtable[23] also
calls mode=12 ("unchanged complement"), but using the same QRegion for both causes
mode=12 to override mode=7, telling framegen nothing changed.

### Files Modified (from upstream timower/rM2-stuff)

| File | Change |
|------|--------|
| `libs/rm2fb/Versions/Version3.25.cpp` | New — full 3.25 implementation |
| `libs/rm2fb/Versions/Version.cpp` | Added 3.25 build ID + ELF PT_NOTE parsing |
| `libs/rm2fb/Versions/Version.h` | Declared `version_3_25_0` |
| `libs/rm2fb/PreloadHooks.h` | Added `Mmap` hook to HOOKS(X) macro |
| `libs/rm2fb/CMakeLists.txt` | Added Version3.25.cpp to build |

### Version.cpp: ELF Build ID Parsing

The original code hardcoded the build-id address at `0x10180`. In 3.25, the linker
placed `.note.gnu.build-id` at a different offset. The fix parses PT_NOTE program
headers from the in-memory ELF to find the GNU build-id note dynamically, with
fallback to the legacy address for older firmware.

## Installation

### Prerequisites

- reMarkable 2 with firmware 3.25.1.1
- SSH access (`ssh root@10.11.99.1`, password on Settings > General > Help)
- Auto-updates disabled (fakeupdateengine or equivalent)
- Nix with flakes enabled (for cross-compilation on macOS/Linux)

### Build from Source

```sh
git clone https://github.com/timower/rM2-stuff.git /tmp/rm2-stuff-src
cd /tmp/rm2-stuff-src
# Apply the 3.25 patches (5 files above)
nix build .#dev-cross
```

### Deploy

```sh
# Prepare binaries
cp result/lib/librm2fb_server.so /tmp/librm2fb_server.so
cp result/lib/librm2fb_client.so.1.1.0 /tmp/librm2fb_client.so
cp result/bin/rm2fb_server /tmp/rm2fb_server_exe
chmod +w /tmp/librm2fb_server.so /tmp/librm2fb_client.so /tmp/rm2fb_server_exe

# Patchelf for device
nix shell nixpkgs#patchelf -c bash -c '
  patchelf --set-interpreter /lib/ld-linux-armhf.so.3 --set-rpath /opt/lib /tmp/rm2fb_server_exe
  patchelf --set-rpath /opt/lib /tmp/librm2fb_server.so
  patchelf --set-rpath /opt/lib /tmp/librm2fb_client.so
'

# SCP to device
scp /tmp/rm2fb_server_exe root@10.11.99.1:/opt/bin/rm2fb_server
scp /tmp/librm2fb_server.so root@10.11.99.1:/opt/lib/librm2fb_server.so
scp /tmp/librm2fb_client.so root@10.11.99.1:/opt/lib/librm2fb_client.so.1.1.0

# Fix libevdev dependency
ssh root@10.11.99.1 'ln -sf /opt/lib/libevdev.so.2 /usr/lib/libevdev.so.2'
```

### Install Helper Script

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

### Verify

```sh
ssh root@10.11.99.1
terminal   # should show yaft on e-ink screen
exit       # should return to reMarkable UI
```

## Reverse Engineering Notes

### How EPFramebufferSwtcon Was Found in xochitl

RTTI strings confirmed the classes exist in xochitl (not libqsgepaper):

```
strings /tmp/xochitl | grep -i swtcon
→ 19EPFramebufferSwtcon
→ 19EPFramebufferFusion
→ SWTCON initialized \o/
```

### How Addresses Were Discovered

1. **EPFramebuffer::instance() (0x583020):** Found via cross-references to
   `operator new(140)` and the "SWTCON initialized" string.

2. **actualUpdate (0x57e6f0):** Found via Fusion vtable[23] disassembly.
   Takes `(void* this, QRegion& region, int waveform, int mode, int flags)`.
   Ignores `this`; uses global swtconData pointer at 0x013240E4.

3. **processAndSignal (0x57d5e4):** Called immediately after actualUpdate in
   vtable[23]. No args — accesses globals directly. Sequence: copy msgs from
   +0x58BC queue → +0x48 processed list, reset queue, unlock mutex at +0x54,
   atomic increment futex at +0x30, futex_wake.

4. **Fusion vtable[23] (0x57fffc):** The update entry point. Creates two QRegions
   via intersection/subtraction, calls actualUpdate twice (mode=7 for changed,
   mode=12 for unchanged). We call actualUpdate once with mode=7 only.

5. **Build ID:** Extracted via `readelf -n /tmp/xochitl` or `xxd -s 0x170 -l 20 -i`.

### Failed Approaches

1. **triggerUpdate (vtable[24], 0x56d978):** Sets pending flag + signals condvar.
   Internal update thread wakes but finds no dirty regions → no UpdateMsgs created.

2. **Dual actualUpdate (mode=7 + mode=12):** Using the same QRegion for both
   causes mode=12 to override, telling framegen nothing changed.

3. **Checking /dev/fb0 for output:** SWTCON does NOT write to the /dev/fb0 mmap.
   The pan buffer content is stale from boot. LCDIF interrupt count is the correct
   way to verify display activity.

### Useful Diagnostics

```sh
# LCDIF interrupt count (increases with each e-ink refresh cycle)
cat /proc/interrupts | grep lcdif

# rm2fb server logs
journalctl -u rm2fb --no-pager -n 50

# Shared framebuffer content
dd if=/dev/shm/swtfb.01 bs=4096 count=1280 | od -x | grep -v "ffff.*ffff.*ffff.*ffff.*ffff.*ffff.*ffff.*ffff" | wc -l

# SWTCON temperature (responds when hardware is active)
cat /sys/class/hwmon/hwmon1/temp0
```

### Tools Used

- **Ghidra** for initial xochitl disassembly
- **Python + capstone** for targeted ARM disassembly from macOS
- **Nix flake** for armv7l cross-compilation on aarch64-darwin
- **patchelf** for fixing interpreter/rpath on nix-built binaries
