# rM2-stuff — reMarkable 2 Firmware 3.25 Support

Fork of [timower/rM2-stuff](https://github.com/timower/rM2-stuff) with firmware 3.25.1.1 support.
Upstream PR: https://github.com/timower/rM2-stuff/pull/50

## Device Access

```sh
ssh root@10.11.99.1          # USB connection (password on Settings > General > Help)
```

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
yaft -> RGB565 shared FB -> rm2fb converts to Y8 gray + ARGB32
     -> actualUpdate(0x57e6f0) creates UpdateMsgs
     -> processAndSignal(0x57d5e4) wakes framegen
     -> SWTCON -> LCDIF -> e-ink panel
```

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
| `libs/rm2fb/Versions/Version3.25.cpp` | New — full 3.25 implementation |
| `libs/rm2fb/Versions/Version.cpp` | 3.25 build ID + ELF PT_NOTE parsing |
| `libs/rm2fb/Versions/Version.h` | Declared `version_3_25_0` |
| `libs/rm2fb/PreloadHooks.h` | Added `Mmap` hook |
| `libs/rm2fb/CMakeLists.txt` | Added Version3.25.cpp |

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
