# Swtcon Re-Implementation Plan & Status

Our goal is to sever the dependency on the black-box `libqsgepaper.so` library by fully reversing and re-implementing the Software Timing Controller (swtcon) logic natively in C++.

## Code layout (as of the Phase 4b cleanup)
The `tools/qsgepaper-preload/` native code is split by concern:
- **`swtcon.h`** - public API (`swtcon_init/update/lock/unlock_post/wait/shutdown`),
  unchanged since Phase 1; `main.cpp` only ever includes this.
- **`swtcon.cpp`** - `dlopen`/`dlsym` loading (`load_lib`), `swtcon_init`/
  `swtcon_shutdown` orchestration (wiring native buffers into the library's
  globals, starting/joining the still-library display/worker threads), and
  the `swtcon_dump_waveform`/`swtcon_dump_buffers` A/B debug helpers.
- **`native_init.h`/`.cpp`** - everything owning the init-allocated resources'
  full lifecycle (pid file, statebuffer/gamma table, framebuffer/LUT,
  waveform loading, temperature sensor discovery+polling) - both the
  `native_*` constructors called from `swtcon_init` and the `native_*`
  destructors called from `swtcon_shutdown`.
- **`native_update.h`/`.cpp`** (new) - `swtcon_lock/update/unlock_post/wait`
  and every native leaf reimplementation they depend on (work-item ctor/copy,
  `clamp_update_rect`, `subtract_update_region`, `build_update_batch`, the
  piece-builder, etc.) - see Phase 4/4b below.
- **`qsgepaper_globals.h`** (new) - named struct layouts for the library's own
  global state we read/write directly by address (`resolve_ptr<T*>(addr)`),
  replacing what used to be individual `resolve_ptr<T*>(0x1234)` calls with a
  trailing name comment. Structs model runs of `.bss` we've confirmed are
  *fully contiguous* by exact address arithmetic between independently-known
  fields (cross-checked with `static_assert`, not guessed); isolated globals
  stay as standalone named address constants. `native_update.h` additionally
  defines the update work-item wire format (`WorkItem`, `WorkItemNode`,
  `BatchNode`, `RegionRows`) the same way, replacing raw `+0x2c`-style byte
  offsets throughout the update leaf functions.

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
- **All remaining dlsym'd leaf calls inside `swtcon_init` reversed and replaced
  (previously tracked as "dummy functions" that still called into
  `libqsgepaper` by address):**
  - `frame_buffer_addr` (0x53fd0 → `native_frame_buffer_addr`): address of
    frame slot N within the mmap'd framebuffer.
  - `upload_lut_to_frame_slot` (0x53bc8, renamed from `dummy_func_53bc8` →
    `native_upload_lut_to_frame_slot`): copies the full waveform LUT into one
    hardware-visible frame slot. Called once per slot (0..`g_nFbSizeY`-1,
    i.e. 17 slots) at init; the still-library `worker_thread_func` also calls
    the *library's* copy of this whenever it recycles a slot (Phase 5).
  - `init_temperature_sensor` / `find_temperature_hwmon_path` /
    `read_temperature_raw` / `refresh_temperature_cache` (0x476dc/0x46924/
    0x46644/0x4681c, renamed from `dummy_func_476dc`/`FUN_00046924`/
    `FUN_00046644`/`FUN_0004681c` → `native_init_temperature_sensor` +
    friends in `native_init.cpp`): scans `/sys/class/hwmon/*/name` for
    `sy7636a_temperature`, reads its `temp0` file, and writes
    `(raw - 2.0C)` into the library's cached-temperature global
    `g_flCachedTemperature` (0x66e20) under `g_dwTemperatureMutex` (0x6d180) —
    the same global `get_current_temperature` (Phase 4b) already reads.
  - `pan_and_unblank` / `prime_display` (0x53ebc/0x468f0 →
    `native_pan_and_unblank` / `native_prime_display`): PUT_VSCREENINFO to
    frame slot 16, retry-unblank via FBIOBLANK, refresh the temperature
    cache, then reblank. Without this the frame counters are never seeded.
  - Ghidra functions and the relevant globals (`g_abTemperatureHwmonPath`,
    `g_flCachedTemperature`, `g_dwTemperatureMutex`) were renamed to match.
  - The only remaining by-address library calls inside `swtcon_init` are
    `pthread_create(..., worker_thread_func, ...)` and
    `pthread_create(..., display_thread_func, ...)` — starting the two
    still-library display threads is explicitly Phase 5, not part of init
    itself.

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
byte-identical to the library's on the device. **Confirmed on hardware:** the
screen renders correctly.

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
**Confirmed on real hardware:** the panel refreshes correctly.

## Phase 4: Re-implementing `swtcon_update` [NATIVE — CONFIRMED WORKING ON HARDWARE]
`swtcon_update`, `swtcon_lock`, `swtcon_unlock_post` and `swtcon_wait` are now
re-implemented natively in `swtcon.cpp` (guarded by `#define NATIVE_UPDATE 1`;
set to 0 to A/B against the library exports). All three test update modes
(HQ / medium / clearing) run end-to-end on the emulator with `FBIOPAN` streaming
0..16 per update and a clean exit — i.e. the library worker thread successfully
consumes the batches our native code enqueues, so the intrusive
`std::list` / `std::shared_ptr` layout is byte-compatible.

Mapping to the library:
- **`swtcon_lock`** ↔ `LockSwapMutex` (0x3b690): `pthread_mutex_lock` on the
  update-queue mutex `g_dwUpdateQueueMutex` @0x6709c.
- **`swtcon_wait`** ↔ `WaitForUpdate` (0x3b644): spin on `g_nShutdownRequested`
  @0x6708c / batch list `g_pListIncomingUpdates` @0x67090.
- **`swtcon_update`** ↔ `queue_update` (0x3ccac): builds a 0x5c-byte work item
  (in a 100-byte list node), normalises the damage rect, packs
  flags/mode/temp/pixel_mode, runs `dispatch_update_regions`, picks the LUT via
  `select_waveform_lut`, clips the rect out of the accumulation list and every
  unlocked queued batch (`subtract_update_region`), then enqueues a deep copy
  (`update_item_copy` + `_M_hook`). The inlined work-item destructor (embedded
  `list<int>` free + three shared_ptr releases) is reproduced by `release_sp`.
- **`swtcon_unlock_post`** ↔ `unlock_and_post_swap` / `UnlockAndPostSwapMutex`
  (0x3dd90): clones the accumulation list into a batch node
  (`build_update_batch`), hooks it into the incoming list after any
  worker-claimed batches, frees the originals (`free_update_region_list`),
  resets the accumulation head/count/flag (@0x670c4/@0x670cc/@0x670d0), unlocks
  the mutex and `sem_post`s the display semaphore @0x67068.

Native code owns the control flow, field packing and list/refcount book-keeping.
The heavy **leaf** routines are still called into libqsgepaper by address and are
the subjects of follow-up reversing: `dispatch_update_regions` (0x4fff8),
`select_waveform_lut` (0x4535c), `subtract_update_region` (0x3be10),
`update_item_ctor` (0x3ffd0), `update_item_copy` (0x3e850), `clamp_update_rect`
(0x4fc40), `get_current_temperature` (0x468a4), `build_update_batch` (0x3ea98),
`free_update_region_list` (0x3e540). **Confirmed on hardware:** all three test
update modes (HQ/medium/clearing) render correctly on the device with this
native control flow calling into the library leaf routines.

## Phase 4b: Reversing the `swtcon_update` Leaf Routines [IN PROGRESS]
With the native control flow confirmed correct end-to-end on hardware, the next
step is to reverse and re-implement each remaining leaf routine one at a time
(A/B against the library per-function, same discipline as Phase 3/4). Still
library calls: `dispatch_update_regions` (0x4fff8), `select_waveform_lut`
(0x4535c), `subtract_update_region` (0x3be10), `build_update_batch` (0x3ea98).

Five leaves are now natively reimplemented in `swtcon.cpp`. **Confirmed on
both the emulator and real hardware** (same ARM binary runs on both — the
`build/dev` preset targets ARM/EABI5 directly, no separate toltec build
needed): all three update modes still run end-to-end with clean `EXIT=0`.
- **`update_item_ctor` (0x3ffd0 → `native_update_item_ctor`):** zero-inits the
  0x5c-byte work item to a degenerate rect `{y0=0,x0=0,y1=-1,x1=-1}`, 25°C
  default temp, `pixel_mode=5`, and a self-referencing empty list head; stamps
  the library's own global sequence counter (`DAT_0006d178` @0x6d178) at
  +0x1c. The rect field order is actually `{y0,x0,y1,x1}`, not `{x0,y0,x1,y1}`
  as originally guessed — corrected throughout. The +0x2c placeholder LUT
  shared_ptr is still allocated by the library's tiny inline allocator
  (0x408a8) since it's always immediately replaced by `select_waveform_lut`.
  **Gotcha:** 0x408a8's real signature is
  `(void* out_sp, int size_kb, int mode_width, int bit_depth, float temperature)`
  — `temperature` is passed in a VFP register (`s0`) per AAPCS-VFP, not as a
  5th integer arg. Ghidra's decompiler mis-groups this as `(int, void*, int,
  int, uint)`, which silently shifts every register by one and writes through
  a null `out_sp` — SIGSEGV. Always check the disassembly's actual register
  wiring against Ghidra's guessed C signature when a function mixes float and
  pointer/int args.
- **`clamp_update_rect` (0x4fc40 → `native_clamp_update_rect`):** takes the
  input rect (queue_update reorders `update_data`'s y/x/height/width fields to
  x0,y0,x1,y1 — the "height"/"width" fields are actually the opposite-corner
  coordinates, not sizes, confirmed by main.cpp's full-screen requests setting
  them to `SCREEN_WIDTH`/`SCREEN_HEIGHT` which match the 1403/1871 constants
  below exactly) and flips it into the panel's 180°-rotated hardware frame: an
  independent per-axis point reflection through `(SCREEN_HEIGHT-1,
  SCREEN_WIDTH-1)` = `(1871, 1403)`, with the y-axis min/max rounded to 8-row
  blocks (down/up) to match the 8-row-aligned render/dispatch kernels.
- **`get_current_temperature` (0x468a4 → `native_get_current_temperature`):**
  just a mutex-protected read of the library's own cached temperature global
  (`DAT_00066e20` @0x66e20, guarded by `DAT_0006d180` @0x6d180). The value is
  still produced by the library's background poll thread (`FUN_0004681c`:
  reads a hwmon sysfs path via `fopen`/`strtol`, subtracts a 2.0°C calibration
  offset) — Phase 5 hasn't reimplemented that thread natively yet, so this
  reads the library's cache directly rather than re-polling hwmon.
- **`update_item_copy` (0x3e850 → `native_update_item_copy`):** deep-copies a
  work item: *retains* (atomically increments use-count on, via the new
  `retain_sp` — the mirror-image of `release_sp`) all three shared_ptrs rather
  than moving them, deep-copies the embedded `std::list<int>` at +0x48 (new
  0xc-byte nodes carrying only the int payload, relinked in source order via
  a plain circular-list tail-append — no need for the library's `_M_hook` here
  since we're building the list from scratch), and copies every other field
  verbatim (rect, sequence id, LUT-width shorts, mode, temperature,
  sync/full-refresh flags, pixel_mode). No register-mapping surprises here —
  every argument is an int/pointer, so Ghidra's decompiled signature matched
  the real ABI exactly.
- **`free_update_region_list` (0x3e540 → `native_free_update_region_list`):**
  walks a circular intrusive list of 100-byte nodes (item lives at node+8) and
  for each: frees the embedded `std::list<int>` at item+0x48, releases the
  three shared_ptrs (+0x00, +0x2c, +0x3c — the exact same trio
  `swtcon_update`'s inline work-item destructor already handled via
  `release_sp`), then frees the node. This is literally the same
  destructor logic already written inline in `swtcon_update`, just looped
  over an entire list instead of one stack item.

### `subtract_update_region` (0x3be10) and `build_update_batch` (0x3ea98) — natively reimplemented
Both are now native (`native_subtract_update_region` / `native_build_update_batch` in
`swtcon.cpp`), unblocked by reversing `dispatch_update_regions`'s output struct
first (see below). **Confirmed on the emulator:** all three update modes
(HQ/medium/clearing) still run end-to-end with clean `EXIT=0` after swapping
both leaves to native. Hardware re-confirmation is still pending (not run this
round). Note the emulator's `main.cpp` test only ever queues one full-screen,
non-overlapping update at a time (each `do_update` call is `Sync`, draining
the accumulation list before the next), so this test exercises the
no-overlap/single-node path and `build_update_batch`'s clone-on-drain path,
but not `subtract_update_region`'s multi-piece splitting logic (which only
triggers when a *new* rect partially overlaps an *already-queued, not-yet-locked*
region) — that path still wants a dedicated overlapping-updates test.

`dispatch_update_regions` (0x4fff8) itself is **not** reimplemented — it's a
full custom thread-pool dispatcher (spins up `hardware_concurrency()` worker
threads via a task queue, or falls back to a single synchronous call, to
`render_update_kernel` @0x4e7b8, which is the actual per-pixel diff kernel).
Porting the dispatcher *and* the pixel kernel is a much bigger undertaking than
the other leaves and isn't required for anything else, so it's likely to stay
library-native for a while. What *was* needed and is now confirmed: its output
struct layout. It allocates a 0x28-byte control block (shared_ptr control
block + an aliased `RegionRows` sub-struct at +0xc) and stores it into the new
item's +0x00 shared_ptr; the *raw pointer* half of that shared_ptr (item+0x00)
points at the `RegionRows` sub-struct itself:
```
RegionRows (via item+0x00's raw pointer):
  +0x00 dataPtr   (uint8_t*, size = stride * (x1-x0+1), column-major:
                   address(y,x) = dataPtr + stride*(x-x0) + (y-y0))
  +0x04 y0  +0x08 x0  +0x0c y1  +0x10 x1
  +0x14 stride  (round_up(y1-y0+1, 16) — the field `native_piece_builder`'s
                 gap-offset math reads)
  +0x18 size
```
item+0x08 (previously an unexplained "gap" field) is simply `*rawPtr`, i.e. a
cached copy of `RegionRows::dataPtr` — but re-based per item to that item's own
rect origin, which is exactly what `native_piece_builder`'s
`stride*(piece.x0-old.x0) + (piece.y0-old.y0)` adjustment maintains when a
region gets split into pieces with a new origin.

`FUN_000400a8` (the piece-builder `subtract_update_region` calls once per
emitted piece) is now `native_piece_builder`: it's `update_item_copy` plus (1)
overwrite the rect with the piece's rect, (2) stamp a *new* sequence id
(unlike `update_item_copy`, which preserves the source's id), (3) apply the
`RegionRows`-relative gap adjustment above.

#### `subtract_update_region` — algorithm
This is the heaviest leaf so far: a classic AABB rectangle-subtraction that
carves the new update's rect out of every overlapping pending region, walking
the accumulation (or a claimed batch's) list. Ghidra's decompiled C for this
one is **actively misleading** in two places (it merges non-equivalent jump
targets that happen to share a label at the assembly level, e.g. `LAB_bf70`
gets reached both by a genuinely-dead fallback path *and* by the legitimate
"only one piece, already stored" path, with different register liveness each
time) — ground truth came from `mcp__ghidra__disassemble_function`, not the
pseudocode. Verified register-level algorithm:
- Per node: AABB overlap test (skip node untouched if no overlap; the checks
  also cover degenerate rects defensively, though those can't occur given the
  upstream `clamp_update_rect` invariants).
- Compute the intersection ("cut") rect: `cut.y0=max(new.y0,old.y0)`,
  `cut.x0=max(new.x0,old.x0)`, `cut.y1=min(new.y1,old.y1)`,
  `cut.x1=min(new.x1,old.x1)`.
- If the cut rect equals the old rect (full containment), the node is removed
  outright — no replacement pieces.
- Otherwise, emit up to 4 leftover axis-aligned strips, each only if
  non-empty, in this fixed order (left, top, bottom, right), each a full copy
  of the old item with only the rect replaced:
  - **left**: `{y0=old.y0, x0=old.x0, y1=old.y1, x1=cut.x0-1}` (full old
    y-range) — only if `old.x0 < cut.x0`.
  - **top**: `{y0=old.y0, x0=cut.x0, y1=cut.y0-1, x1=cut.x1}` (cut's x-range
    only, since left already claimed the old x-range) — only if
    `old.y0 < cut.y0`.
  - **bottom**: `{y0=cut.y1+1, x0=cut.x0, y1=old.y1, x1=cut.x1}` — only if
    `cut.y1 < old.y1`.
  - **right**: `{y0=old.y0, x0=cut.x1+1, y1=old.y1, x1=old.x1}` (full old
    y-range again) — only if `cut.x1 < old.x1`.
- For 1+ pieces: the old node is unhooked and freed (via the same
  free-embedded-list + release-3-shared_ptrs + delete pattern as
  `free_update_region_list`'s per-node body), a temp copy of the old item is
  taken first (`update_item_copy`) to preserve its LUT/mode/temp/flags, then
  each piece becomes a new 100-byte node built by `FUN_000400a8` (see below)
  from that temp copy + the piece's rect, hooked into the list in place of the
  old node.

`build_update_batch` shares the same clone pattern as `update_item_copy`, but
per-node (not per-piece) and without the rect/seq/gap adjustment — it's a
straight list-of-items clone into a fresh batch node (list head + sub-list +
count + mode-short-from-accum's-flag), hooked into the incoming list at a
given position. See `native_build_update_batch` in `swtcon.cpp`.

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
