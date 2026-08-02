# libqsgepaper 3.23 → 3.27 diff notes

Companion to `swtcon_architecture.md` (which documents `libqsgepaper_3.23.0.54.so`
exclusively). This file tracks what changed in `libqsgepaper_3.27.1.0`,
found by decompiling both binaries in Ghidra and diffing. Same confidence
tags as `swtcon_architecture.md`: **[confirmed]** (read directly from
disassembly/decompile), **[derived]** (reasoned from confirmed facts, not
independently checked), **[guess]** (plausible, unverified).

Programs, both in the `rm-shutdown` Ghidra project:
- `libqsgepaper_3.23.0.54.so`
- `libqsgepaper_3.27.1.0`

**Ghidra state for `libqsgepaper_3.27.1.0` reflects this doc as of this
pass** — every function identified below with a confirmed 3.23 counterpart
has been renamed in the project to match (via `rename_function_by_address`),
and `DAT_00066fe8` is typed/named as `g_pSwtconState`. Renamed:
`dispatch_update_regions` (`0x4cc48`), `render_update_kernel` (`0x4afe4`),
`advance_work_item_frames` (`0x3bf5c`), `display_thread_func` (`0x3e62c`),
`dispatch_processed_regions` (`0x4d2e0`), `dispatch_plain_kernel`
(`0x5229c`), `dispatch_overlap_kernel` (`0x521f8`), `playback_kernel_plain`
(`0x55150`, manually created as a function first — see §5d), and
`playback_kernel_overlap` (`0x55244`, same). Two functions were
deliberately **left unrenamed** with a plate comment instead —
`0x48808`/`0x486bc`, the candidate `commit_kernel_incremental`/
`commit_kernel_force` — because `0x48808`'s body size (10203 bytes) is
~21× its 3.23 counterpart (479 bytes) and hasn't been decompile-diffed; see
§5b and Next steps.

The headline change: **3.27 switched the shared xochitl/swtcon framebuffer
from 16bpp (RGB565) to 32bpp (RGB32/xRGB8888), end to end** — buffer size,
`QImage` format, and the per-pixel diff kernel's pixel decode all moved
together. Everything else checked (control flow, threading/chunking
structure) is unchanged.

---

## 1. Init entry point renamed, gained a parameter — **[confirmed]**

| | 3.23 | 3.27 |
|---|---|---|
| Symbol | `qsgepaper_init` | `swtcon_init` |
| Address | `0x3b814` | `0x3f270` |
| Signature | `int qsgepaper_init(InitParams*, int)` | `int swtcon_init(InitParams*, int, const char*)` |

Both are real exported symbol names (not Ghidra `FUN_`/reverser-assigned —
same status as `swtcon_init` elsewhere in this project's docs, which
anticipated this rename). The 3.27 wrapper (`EPFramebufferSwtcon::initialize`,
see below) calls it with the new third argument as `""`; purpose unconfirmed
— likely a waveform/profile override name, a plausible **[guess]** given
`qsgepaper_init`'s own second `int param_2` argument already feeds
`FUN_0004fd04`/`FUN_0004c880` (a param seen but not chased down in either
version).

The wrapper that calls it, `EPFramebufferSwtcon::initialize`, is **[confirmed]
byte-for-byte identical in structure** between versions (`0x38e30` in 3.23,
`0x3916c` in 3.27) — same `InitParams` field layout, same post-init branch:

```c
if (depth_ == 0) { bytesPerLine = stride << 1; format = 7;  /* QImage::Format_RGB16 */ }
else              { bytesPerLine = stride << 2; format = 4;  /* QImage::Format_RGB32 */ }
```

`InitParams` field layout (offsets from struct base, both versions):

| Offset | Field |
|---|---|
| `0x00` | `width` |
| `0x04` | `height` |
| `0x08` | `stride_` |
| `0x0c` | `depth_` |
| `0x10` | `foo` (unused/padding) |
| `0x14` | `buf1` (the "image" plane — RGB16 or RGB32 depending on `depth_`) |
| `0x18` | `buf2` (the grayscale mask plane, `Format_Grayscale8`, unaffected by `depth_`) |

## 2. `depth_` is now hardcoded to 1, was hardcoded to 0 — **[confirmed]**

This is the actual switch. The wrapper branch above is identical in both
binaries — what changed is which value `qsgepaper_init`/`swtcon_init` writes
into `depth_`:

- **3.23**: `param_1[1] = 0x57c;` — a single 8-byte store that sets
  `stride_ = 0x57c` **and**, as a side effect of the store width, zeroes the
  adjacent `depth_` field in the same instruction. Always 16bpp; the 32bpp
  branch in `EPFramebufferSwtcon::initialize` is dead code on this version.
- **3.27**: separate assignments —
  `width = 0x57c; height = 0x750; stride_ = 0x57c; depth_ = 1;` — explicit,
  standalone store to `depth_`. Always 32bpp now; the 16bpp branch is what's
  dead on this version.

Not a runtime panel-capability check either way — both versions hardcode the
value; 3.27 just hardcodes the other branch.

## 3. Primary buffer allocation doubled — **[confirmed]**

The buffer `qsgepaper_init`/`swtcon_init` allocates for `buf1` (the "image"
plane swtcon's kernels write into, called `dataBuffer` in
`swtcon_architecture.md`):

| | 3.23 | 3.27 |
|---|---|---|
| Allocation | `operator_new[](0x503580)` | `operator_new[](0xa06b00)` |
| Bytes/pixel (nominal, `1404×1872`) | ~2 | ~4 |

Ratio is ≈2.00×, consistent with 16bpp→32bpp. (Neither figure is an exact
`1404*1872*N` — both have some stride padding/rounding baked in; not chased
further.) `buf2` (the grayscale mask plane, `0x281ac0` bytes, 1 byte/pixel)
is unaffected in both versions.

So this isn't just the `QImage` wrapper reinterpreting the same bytes — the
backing buffer that `render_update_kernel`'s equivalent actually reads is
natively 32-bit-per-pixel in 3.27.

## 4. Per-pixel diff kernel rewritten for 32-bit input — **[confirmed]**

Located via cross-binary fuzzy matching
(`mcp__ghidra__find_similar_functions_fuzzy`), since neither function keeps
its 3.23 Ghidra-assigned name in 3.27:

- `dispatch_update_regions` (3.23 `0x4fff8`) → `FUN_0004cc48` (3.27), fuzzy
  score 0.67 — same two-chunk thread-pool submission structure as
  `swtcon_architecture.md` §5.1 describes.
- Its per-item worker → `FUN_0004afe4` (3.27), the 3.27 equivalent of
  `render_update_kernel` (3.23 `0x4e7b8`, `swtcon_architecture.md` §5.2).

Pixel decode, old vs new:

```c
// 3.23 — 16-bit src, RGB565-shaped bitfields
lo5  = src & 0x1f;
mid6 = (src & 0x7ff) >> 5;
hi5  = src >> 0xb;
// summed, + gamma, /125, *30  (case 8), or *15/125<<1 (case 9), etc.

// 3.27 — 32-bit src, 0x00RRGGBB, BT.601-ish luma
R = (px & 0xffffff) >> 0x10;
G = (px & 0xffff) >> 8;
B =  px & 0xff;
luma = (R*0x4d + G*0x96 + B*0x1d) >> 8;   // weights 77/150/29, standard luma scale
```

The gated case (pen/marker, 3.23's case 7) also changed shape: 3.23 produces
a multi-level output (`(sum+gamma)/125 * 30`, several discrete steps); 3.27's
equivalent does a single threshold compare against gamma
(`luma_partial + gamma < 0xff ? 0 : 0x1e`) — a binary (two-level) decision
rather than the old multi-step quantization. **Case-number correspondence
now resolved, see §6**: 3.27's gated formula is case **8**, not case 7 —
the gated/unconditional roles are swapped relative to 3.23 (3.23: case 7
gated, case 8 unconditional; 3.27: case 7 unconditional, case 8 gated).

**Open / not chased down — [guess]:** the gamma/dither table indexing in
3.27 uses `& 0x3f` with a `0x48`-byte row stride, vs 3.23's `& 0x7f` /
`0x88`-byte stride (`swtcon_architecture.md` §5.2's `g_pGammaTable`). Could
mean the dither matrix shrank from 128×128 to 64×64 in 3.27, or could be an
artifact of reading a NEON-unrolled sub-index rather than the raw table
coordinate directly — not verified against the raw table bytes on either
binary.

## 5. Display-thread dispatch & playback kernels — control flow/constants unchanged, `WorkItem` layout and state-pointer shape shifted

Chased the call chain `display_thread_func` → `dispatch_processed_regions` →
commit kernel, and `advance_work_item_frames` → playback kernel
(`swtcon_architecture.md` §6.2/§6.4), the same way as §4. Found by walking
call sites this time rather than fuzzy-matching alone, so confidence is
higher than §4.

### 5a. `display_thread_func` — **[confirmed]**, structurally identical

3.23 `display_thread_func` (`0x3d2ac`) → 3.27 `FUN_0003e62c` (confirmed by
direct decompile, not just fuzzy score — body sizes match closely, ~0xa5b
vs ~0xa9b bytes). Every stage from `swtcon_architecture.md` §6.2 is present
and recognizable: stale-row cleanup (15-back/mod-16 bucket loop), the
`g_pListProcessedUpdates` GC pass, incoming-batch trylock intake with the
dependency-list gate-check, a call into the `dispatch_processed_regions`
equivalent (below), and the frame-pacing/commit step.

The frame-pacing formula (§6.2 step 5) is **byte-identical**, magic
constants included: `workloadSum = Σ(((y1-y0+1)*(x1-x0+1))<<3)/1000`,
`budget = workloadSum+100`, `paceTarget = (minX0+1)*0x1d96/1000`, and the
`0x2df3`/`0x2df1`/`0x5be3` elapsed-time thresholds all appear verbatim in
`FUN_0003e62c`. The 32bpp switch touched none of this — expected, since
frame pacing operates on rect geometry and wall-clock time, not pixel
format.

### 5b. `dispatch_processed_regions` — **[confirmed]**, same gates, same kernel selection

Found by following the call `FUN_0004d2e0(piVar19)` directly out of
`FUN_0003e62c` (3.23: `dispatch_processed_regions` at `0x50660`, called the
same way out of `display_thread_func`). Confirmed structurally, not just by
fuzzy score (which was a middling 0.48 — the decompiled body is
considerably smaller than 3.23's, ~0x7f3 vs ~0x1257 bytes, likely just a
less-unrolled/differently-optimized build rather than an algorithm change):

- Same per-item chunk-count gate: item width `< 0x1d` (29) → 1 chunk, else
  2 — identical threshold to 3.23's "width against a threshold of 29"
  (`swtcon_architecture.md` §6.2 step 4).
- Same per-item commit-kernel selection on the item's `sync` flag: a byte
  read at `item+0x74` (`piVar13[0x1d]`) picks `FUN_00048808` (`sync==0`,
  "incremental") vs `FUN_000486bc` (`sync!=0`, "force") — the 3.27
  counterparts of 3.23's `FUN_0004f8f0`/`FUN_0004e680`. Not decompiled yet;
  flagged below.

### 5c. `advance_work_item_frames` and the playback-kernel dispatch wrappers — **[confirmed]**, same structure

3.23 `advance_work_item_frames` (`0x3a984`) → 3.27 `FUN_0003bf5c` (fuzzy
score 0.57, and confirmed by call site — `FUN_0003e62c` calls it exactly
where §6.2 steps 6/7 call `advance_work_item_frames`). It dispatches to two
wrapper functions depending on the same kind of overlap-dependency/
alignment check 3.23 used: `FUN_0005229c` and `FUN_000521f8` (3.27
counterparts of 3.23's `dispatch_aligned_kernel`/`dispatch_plain_kernel`,
`FUN_0003f294`/`FUN_0003f1f0`). Both wrappers share an identical chunk-count
gate — `(width+1)*(height+1) > 20000` and `width < 10` → 1 chunk else 2 —
the same shape as `dispatch_update_regions`'s and `dispatch_processed_regions`'s
own gates elsewhere in this pipeline, just with different constants.

### 5d. The playback kernels themselves — **[confirmed]**, had to be manually recovered in Ghidra

The two wrappers above each submit a task carrying a raw function pointer —
`&LAB_00055150` and `&LAB_00055244` — into the shared thread pool, the same
"reached only via a function pointer in a task object" pattern 3.23's
`FUN_0004a140`/`FUN_0004a234` had. **Ghidra's auto-analysis in 3.27 hadn't
recognized either address as a function** — `get_function_by_address` came
back empty and `disassemble_bytes` reported the bytes as already-classified
data, even though the bytes are genuine ARM code (confirmed by hex-dumping
and hand-reading the prologue/jump-table opcodes). Created them as
functions manually (`mcp__ghidra__create_function`) before they'd
decompile: `FUN_00055150` (body 10140 bytes) and `FUN_00055244` (body 17232
bytes) — sizes in the same ballpark as 3.23's plain/overlap kernels.

Both have the same `switch(frameCount)` shape as 3.23 (cases 0 through 8
present in `FUN_00055150`, confirmed directly in its decompile).

**Destination addressing is unchanged — [confirmed]:** `FUN_00055150`'s
case 1 writes through
`frameSlots[k] + ((col+3)*0x104 + (rectY0>>3) + group + 0x1a) * 4` —
the same `0x104` stride and `+3`/`+0x1a` offset constants as 3.23's
`playback_kernel_plain` (`swtcon_architecture.md` §6.4). Makes sense: this
addresses the *panel-drive* frame slots (2-bit-per-phase output), which is
downstream of color-format conversion, not raw pixel data.

### 5e. The playback kernel algorithm itself — **[confirmed] unchanged**

Resolved by decompiling 3.23's actual `playback_kernel_plain` (`0x4a140`,
already named/typed from earlier reversing work — a real docstring and
`WorkItem*`-typed signature, not a bare `FUN_`) and line-diffing it against
`FUN_00055150`'s case 1. They match **statement-for-statement**, including
every NEON intrinsic call and every magic constant
(`0xfff2fff4fff6fff8`/`0xfffafffcfffe0000`, `0x3000300030003`, the
`(7-row)*2` OR-accumulation into the 16-bit frame word). Spot-checked the
"aligned" kernel too (`FUN_00055244` vs. 3.23's `playback_kernel_overlap`
case 1): same LUT-index formula, and its destination address —
`*param_1 + (rectX0*0x104 + (rectY0>>3) + 0x326) * 4` — is the plain
kernel's `(col+3)*0x104 + ... + 0x1a` formula with `col+3` symbolically
expanded (`3*0x104 + 0x1a = 0x326`, exactly). **The per-pixel drive-value
algorithm did not change in 3.27** — same LUT-word indexing, same 2-bit
packing, same destination addressing, in both kernels.

What *did* change, now pinned down precisely by this diff:

- **`WorkItem`'s field layout shifted by a uniform `+0x18` (24 bytes)**,
  confirmed by comparing named-field accesses in 3.23 against their offset
  equivalents in 3.27:
  - `item->lut` (3.23: `+0x2c`) → `+0x44` in 3.27.
  - `item->pStateDataPtr` (3.23: `+0x44`) → `+0x5c` in 3.27.
  - rect fields (3.23: `+0x0c..+0x18`) → `+0x24..+0x30` in 3.27.

  All three shift by exactly `0x18`, so this is one contiguous ~24-byte
  insertion somewhere before `regionRows`/`gap`/the rect block in the
  struct, not scattered per-field changes — but *what* got inserted there
  isn't identified yet (see Next steps).
- **`LUTEntry` gained a field.** 3.23's `LUTEntry` is
  `{size_kb, mode_width, temperature, bit_depth, data}` with `data` at
  `+0x10` (`swtcon_architecture.md` §1). In 3.27, `bit_depth` is still read
  from the same relative slot (`+0xc`), but `data` moved to `+0x14` — one
  new 4-byte field inserted between `bit_depth` and `data`. Meaning
  unconfirmed; a plausible **[guess]** given the timing is a per-LUT
  color-depth/format tag, but not verified.

Neither change touches the kernels' own math — both are pure data-layout
growth that the kernels just read through at different offsets. The
playback stage is otherwise untouched by the 32bpp switch, consistent with
it operating purely on already-quantized transition state, several stages
downstream of the RGB pixel decode that §4 found changed.

---

## 6. Auto-mode `pixelMode` dispatch table — **[confirmed] mode numbers do NOT match**

Resolved by decompiling 3.23's actual `render_update_kernel` (`0x4e7b8`,
already named/typed from earlier work) and reading both dispatch tables'
raw bytes out of the loaded binaries, rather than guessing from case shape
alone.

**The auto-mode sentinel value itself changed, 5 → 6:**

```c
// 3.23, render_update_kernel:
iVar19 = item->pixelMode;             // WorkItem+0x58
if (iVar19 == 5) {                     // sentinel: 5
  uVar20 = (item->mode - 1) & 0xffff;
  if (6 < uVar20) goto default;        // valid: mode 1..7 (7 entries)
  iVar19 = g_anPixelModeDispatchTable[uVar20];
}

// 3.27, render_update_kernel:
iVar13 = item->pixelMode;             // WorkItem+0x70 (shifted, §5e)
if (iVar13 == 6) {                     // sentinel: 6
  uVar27 = (item->mode - 1) & 0xffff;
  if (5 < uVar27) goto default;        // valid: mode 1..6 (6 entries)
  iVar13 = DAT_0005f158[uVar27];
}
```

Raw table bytes (`inspect_memory_content`, both binaries):

| `item->mode` | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| 3.23 → case (`g_anPixelModeDispatchTable` @ `0x596b8`) | 6 | 9 | 9 | 9 | 9 | 6 | 8 |
| 3.27 → case (`DAT_0005f158` @ `0x5f158`) | 7 | 0xc | 0xc | 0xc | **9\*** | 7 | *(no entry — 6-entry table)* |

`*` mode 5 does not share modes 2–4's formula in 3.27, see below.

**The raw case numbers are meaningless across versions on their own** —
what matters is which *formula* each case implements, and there the switch
statement itself was reshuffled:

- **Case 7 and case 8 swapped roles.** 3.23: case 7 = gated (backBuffer/mask
  check, pen-and-marker formula), case 8 = unconditional gamma-dithered
  formula, case 6 = bare `goto` into case 8. 3.27: case 7 = unconditional
  gamma-dithered formula (no gate), case 8 = gated. So **modes 1 and 6 keep
  their semantic meaning** ("unconditional dither") across versions — 3.23
  routes them to case 6→8, 3.27 routes them to case 7 directly — just under
  a different case number, because the two formula bodies traded case-label
  slots (most likely an artifact of the compiler reordering switch cases
  around the reshuffled case list caused by §4's pixel-format rewrite, not
  a deliberate renumbering).
- **Modes 2–4 keep their formula shape**, just rescaled. 3.23 case 9:
  `((sum*15+gamma)/125)<<1`. 3.27 case `0xc`: same shape,
  `((luma*15+gamma)/255)<<1` — divisor rescaled `125→255` for the new
  8-bit-per-channel luma range (§4), not a behavior change.
- **Mode 5 diverges.** In 3.23, modes 2–5 are all the *same* table entry
  (`9`) and share case 9's formula uniformly. In 3.27, modes 2–4 map to
  case `0xc` (case 9's rescaled equivalent) but **mode 5 maps to bare case
  `9`, which is just `goto LAB_0004b0bc; FUN_00048590(rectY0, rectX0,
  rectY1, chunkCount); return;`** — a tail call into a separate,
  not-yet-reversed function, not an inline formula. Cases 2, 3, and 10 also
  tail-call the same `FUN_00048590`. Whether `FUN_00048590` actually
  produces the *same* output as case `0xc`'s inline formula (just factored
  out, e.g. for register-pressure reasons in a heavily-NEON-unrolled
  function) or a genuinely different one is **[open]** — not decompiled
  yet. It sits immediately before the two commit-kernel candidates from §5b
  (`0x48590`–`0x486bb`, immediately followed by `0x486bc`), which is
  probably just linker layout, not a relationship — flagged in case it
  turns out not to be.
- **Explicit (non-auto) `pixelMode` literal 6 changed formula identity —
  relevant to this repo's own code.** 3.23: literal `pixelMode=6` is the
  bare alias into case 8 (gamma-dithered, unconditional). 3.27: case 6 in
  the switch is an empty `break`, which falls through to the **shared
  `default` block** — the plain, no-gamma `luma>>0xb & 0xfe` quantization
  (§4's "default" formula), a third, distinct formula. `libs/rm2fb/
  ServerSwtcon.cpp:47` explicitly sets `pixel_mode = 6` for the pen/priority
  path, with a comment explaining *why 6* ("Don't use 7, as that'd use the
  backBuffer, which is not set") — reasoning grounded specifically in
  3.23's case-6-aliases-case-8 behavior. This is **not a live bug**:
  `ServerSwtcon.cpp` drives this repo's own native `libs/swtcon` port
  (reversed from 3.23), which never calls into any real `libqsgepaper.so`,
  so 3.27's real-library behavior is irrelevant to it today. But it does
  mean **the literal value `6` is not a stable cross-version ABI constant**
  — worth remembering if this project ever needs to target or A/B against
  the real 3.27 library specifically, since the same value now selects a
  visibly different (non-dithered) formula there.

---

## Next steps

1. Verify the gamma-table dimension question (§4) by reading the raw table
   bytes in 3.27 the way `swtcon_architecture.md` §5.2 did for 3.23
   (`render_kernel_verify.cpp`-style multiset check, or a direct memory dump
   at the 3.27 table address).
2. ~~Re-derive 3.27's auto-mode pixelMode → case dispatch table~~ — done,
   see §6. Headline: sentinel changed `5→6`, table shrank `7→6` entries,
   case 7/8 swapped roles, but modes 1/6's formula and modes 2-4's formula
   (shape) both survived under the new numbering. Follow-up:
   - Decompile `FUN_00048590` (§6, the mode-5/case-2/3/10 tail call) and
     compare its output against case `0xc`'s inline formula — the one
     open question left in the auto-dispatch table is whether mode 5
     actually still behaves like modes 2-4 in 3.27, or has genuinely
     diverged.
3. Check `swtcon_init`'s new third parameter (`""` at the only call site
   seen) — grep other callers / xochitl itself for a non-empty value before
   assuming it's unused in practice.
4. ~~Diff `dispatch_processed_regions` and the worker-side playback
   kernels~~ — done, see §5. **Headline result: the playback kernels'
   per-pixel algorithm is unchanged (§5e)** — same LUT-index formula, same
   2-bit packing, same destination addressing, statement-for-statement
   identical to 3.23's `playback_kernel_plain`/`playback_kernel_overlap`.
   Only the surrounding data layout moved. Follow-ups that fell out of that
   pass:
   - Find what got inserted into `WorkItem` to cause the uniform `+0x18`
     shift (§5e) — it lands somewhere before `regionRows`/`gap`/the rect
     block (3.23 offsets `+0x00`/`+0x08`/`+0x0c`). Likely candidate: a new
     field related to the 32bpp switch (e.g. a per-item pixel-format/depth
     tag), but unconfirmed. Cross-check against `update_item_ctor`'s 3.27
     equivalent (not yet located) the way `swtcon_architecture.md` §1
     originally derived the 3.23 layout.
   - Confirm what the new `LUTEntry` field (§5e, inserted before `data`,
     after `bit_depth`) actually holds — read `load_waveform`'s 3.27
     equivalent (not yet located) to see what populates it.
   - Decompile `FUN_00048808`/`FUN_000486bc` (§5b, the commit-kernel pair)
     and diff against 3.23's `FUN_0004f8f0`/`FUN_0004e680` — this is where
     the packed transition-state format (3.23: `(oldState<<5)|newValue`,
     `uint16`) is actually produced. Given §5e found the *consumer*
     (playback kernel) unchanged, the producer is now the best remaining
     place a real format change could still be hiding — worth checking
     even though the read side gives no reason to expect one.
   - Full case-by-case diff of `FUN_00055244` against `FUN_00055150`
     (only case 1 was spot-checked) to confirm the plain/aligned identity
     3.23 has (`swtcon_porting.md`'s `FUN_0004a234` entry) still holds for
     every case in 3.27, not just case 1.
