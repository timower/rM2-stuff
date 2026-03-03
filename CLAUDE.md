# rM2-stuff: Firmware 3.25 Support

## Reproducing This Work

### Prerequisites

- macOS (aarch64) or Linux host with Nix installed and flakes enabled
- reMarkable 2 tablet running firmware 3.25.1.1
- SSH access to the tablet (`ssh root@10.11.99.1`)
- Auto-updates disabled on the tablet
- A copy of the xochitl binary from the device (`scp root@10.11.99.1:/usr/bin/xochitl /tmp/xochitl`)

### Build

```sh
cd /path/to/rM2-stuff
nix build .#dev-cross
```

This cross-compiles for armv7l. Outputs are in `result/`:
- `result/bin/rm2fb_server` — server executable
- `result/lib/librm2fb_server.so` — server shared library (the actual code runs here via LD_PRELOAD)
- `result/lib/librm2fb_client.so.1.1.0` — client library for apps like yaft

### Deploy to Device

```sh
# Copy and fix ELF metadata for device
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

# Fix libevdev dependency (one-time)
ssh root@10.11.99.1 'ln -sf /opt/lib/libevdev.so.2 /usr/lib/libevdev.so.2'
```

### Test

```sh
ssh root@10.11.99.1
systemctl stop xochitl
systemctl start rm2fb
LD_PRELOAD=/opt/lib/librm2fb_client.so.1.1.0 /opt/bin/yaft
```

Expected: yaft terminal visible on e-ink screen, keyboard input works.

### Verify Display Updates

```sh
# LCDIF interrupt count should increase after each screen update
ssh root@10.11.99.1 'cat /proc/interrupts | grep lcdif'

# Server logs
ssh root@10.11.99.1 'journalctl -u rm2fb --no-pager -n 20'
```

## Key Technical Details

### Why LD_PRELOAD Mode

In firmware 3.20-3.23, EPFramebufferSwtcon lived in libqsgepaper.so. The server
could dlopen it and dlsym the update functions. In 3.25, EPFramebufferSwtcon is
statically linked into xochitl. The libqsgepaper.so build ID is intentionally NOT
mapped in Version.cpp so ServerExe falls through to the LD_PRELOAD path.

### IMPORTANT: Deploy the Library, Not Just the Executable

The rm2fb_server executable does `exec /usr/bin/xochitl` with
`LD_PRELOAD=librm2fb_server.so`. The Version3.25.cpp code runs inside the
**library**, not the executable. Always deploy both files.

### Finding Addresses for New Firmware Versions

If a future firmware update changes addresses, use this process:

1. Extract xochitl: `scp root@10.11.99.1:/usr/bin/xochitl /tmp/xochitl`
2. Get build ID: `readelf -n /tmp/xochitl`
3. Find EPFramebuffer::instance(): search for "SWTCON initialized" string xref,
   trace back to the function that calls `operator new(140)`
4. Find actualUpdate: locate Fusion vtable (search for vtable entries near instance()),
   vtable[23] contains the update dispatcher. actualUpdate is the first function it calls
   after locking the mutex.
5. Find processAndSignal: called immediately after actualUpdate in vtable[23].
   Signature: no args, accesses swtconData global directly.
6. Find swtconData pointer: global referenced by processAndSignal (stores at fixed address)
7. Find singleton pointer: set by instance(), referenced by vtable[23]

Tools: Ghidra for full disassembly, or Python + capstone for targeted analysis:
```python
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
with open('/tmp/xochitl', 'rb') as f:
    f.seek(addr - 0x10000)  # xochitl loads at 0x10000
    code = f.read(0x200)
for insn in cs.disasm(code, addr):
    print(f"  0x{insn.address:08x}: {insn.mnemonic} {insn.op_str}")
```

### Allocation Sizes to Watch

These may change across firmware versions:
- Gray buffer: `1404 * 1872 * 1 = 0x281ac0` (has been stable across versions)
- ARGB buffer: `1404 * 1872 * 4 = 0xa06b00` (new in 3.25)
- /dev/fb0 mmap: `0x17BD800` (may vary)
