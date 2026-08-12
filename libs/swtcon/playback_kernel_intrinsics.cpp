#include "display.h"

#include <cstddef>
#include <cstdint>

#include "init.h"   // LUTEntry
#include "update.h" // WorkItem, RegionRows

#define KERNEL_MODE_C 1
#define KERNEL_MODE_ASM 2
#define KERNEL_MODE_NEON 3

// Set by libs/swtcon/CMakeLists.txt; direct compilations that skip it (e.g.
// tools/swtcon-test, arm-only) default to the real production kernel.
#ifndef SWTCON_KERNEL_MODE
#define SWTCON_KERNEL_MODE KERNEL_MODE_ASM
#endif
#define KERNEL_MODE SWTCON_KERNEL_MODE

#if KERNEL_MODE == KERNEL_MODE_NEON && !defined(__ARM_NEON) &&                 \
  !defined(__ARM_NEON__)
#error "Unsupported kernel mode"
#endif

// tools/swtcon-test/playback_kernel_bench.cpp compiles this file three times
// (once per KERNEL_MODE) into one binary to compare them directly, so each
// build needs its own link symbol - defaults to empty (the real, single-mode
// production name) when unset.
#ifndef SWTCON_KERNEL_SYMBOL_SUFFIX
#define SWTCON_KERNEL_SYMBOL_SUFFIX
#endif
#define SWTCON_CONCAT_(a, b) a##b
#define SWTCON_CONCAT(a, b) SWTCON_CONCAT_(a, b)
#define PLAYBACK_KERNEL_PLAIN_FN                                               \
  SWTCON_CONCAT(playback_kernel_plain_intrinsics, SWTCON_KERNEL_SYMBOL_SUFFIX)
#define PLAYBACK_KERNEL_ALIGNED_FN                                             \
  SWTCON_CONCAT(playback_kernel_aligned_intrinsics, SWTCON_KERNEL_SYMBOL_SUFFIX)

// Native reimplementation of FUN_0004a140 (0x4a140, the "plain" playback
// kernel) - fully reversed via a dedicated probe tool (isolated by-address
// calls against the real library with guard-paged buffers, cross-checked
// against its Ghidra decompile - see AGENTS.md). For each column in
// [rectX0,rectX1] restricted to this call's [chunkIndex,chunkCount) column
// sub-range, and each 8-row group in [rectY0,rectY1]:
//   - fetches each of the group's 8 rows' packed LUT word ONCE (word_idx =
//     phase/8, indexed using the row's own transition value - the raw
//     transitionDataPtr u16, used DIRECTLY as the LUT's (row*mode_width+col)
//     index, not split via >>5/&0x1f - the same formula as
//     read_lut_packed_pixel, see WorkItem::transitionDataPtr's comment);
//   - for every one of the 8 possible sub-phases (bit offsets 0-7) packed
//     into that one word, extracts each row's 2-bit value and OR-accumulates
//     it into a per-sub-phase 16-bit word at bit position (7-row)*2 - row 0
//     lands at the TOP of the word, row 7 at the bottom (confirmed
//     empirically via 4 isolated single-row probe experiments, matching the
//     180-degree-rotation convention already established for
//     render_update_kernel elsewhere in this codebase, not a naive lane
//     order);
//   - for k in [0,frameCount): ORs shared-buffer entry (phase&7)+k into
//     frameSlots[k] at the SAME destination address (only the target buffer
//     differs per k). This is why frameCount 1-8 all reduce to the same
//     per-column/per-group computation done once:
//     advance_work_item_frames always picks frameCount so
//     phase&7+frameCount never crosses the 8-sub-phase boundary of a single LUT
//     word (confirmed: case 2's decompile does one shared LUT fetch, not two
//     independent ones).
// Destination byte offset from frameSlots[k]'s base (probe-verified across
// single/multi-group rects and multi-column chunk ranges):
//   ((col+3)*0x104 + (rectY0>>3) + group + 0x1a) * 4
// - a 32-bit-word-per-column stride matching FbInitParams' xres=0x104=260.
// mode_width is read from item->lut, but the 2-bit/8-rows-per-word packing
// constants are hardcoded immediates in the real kernel too (not driven by
// lut->bit_depth at runtime) - matches load_waveform, which always
// produces bit_depth=2 LUTs, so this native port hardcodes the same
// assumption rather than generalizing for a case that never occurs.
//
// Split into its own translation unit (Phase 9, see AGENTS.md/CLAUDE.md) so a
// NEON-vectorized fast path can live alongside the portable scalar one behind
// an #ifdef, without dragging every other display.cpp dependency
// (threads, globals, ...) into the same file.
//
// Non-static (extern, declared in display.h): tools/swtcon-test/
// playback_kernel_bench.cpp calls these directly to isolate the compute cost
// of the kernel from the threading/dispatch machinery around it, and to A/B
// the NEON intrinsics port against the raw-assembly transliteration.

// Stores one group's 16-bit result into a frame slot. Overwrite=false is the
// "plain" kernel (playback_kernel_plain): OR-accumulate over existing content.
// Overwrite=true is the "aligned" kernel (playback_kernel_aligned): store
// directly, like the real library's aligned kernel, the guaranteed first/only
// writer into freshly-zeroed slots at an 8-aligned phase. On zeroed slots the
// two produce identical output; only the store cost differs.
template<bool Overwrite>
static inline void
store_group(uint16_t* dest, uint16_t value) {
  *dest = Overwrite ? value : (uint16_t)(*dest | value);
}

// Computes shared[8]: shared[b] = OR over r=0..7 of
// (((lut_words[r] >> 2b) & 3) << ((7-row)*2)) - i.e. an 8x8 transpose of
// 2-bit fields (row -> subphase becomes subphase -> row), see the file
// header comment above for the full derivation. Two implementations below,
// selected by #ifdef:
//
//   - The NEON path is a direct port of the real library's own strategy for
//     this exact step, read from FUN_0004a140's case-8 (frameCount=8, the
//     dominant real-world case - see AGENTS.md's Phase 9 entry) handler at
//     0x49524 via Ghidra disassembly (not decompile - the compiler-generated
//     pseudocode for this heavily-scheduled function was unreliable to read
//     directly). Unlike an OR-reduce over one subphase at a time (row ->
//     8-lane vector, reduce to scalar, repeat per subphase - discarded after
//     an earlier pass, see git history), the real kernel vectorizes the
//     OTHER axis: one row at a time, hardware-replicated into all 8 lanes
//     via a single "load and duplicate" instruction (VLD1.16 {dX[],dY[]} -
//     vld1q_dup_u16), then extracts ALL 8 subphases from that one row's LUT
//     word in a single per-lane variable shift + mask (confirmed via the
//     constant vector loaded once from a literal pool at the function's
//     entry, reused unchanged for every row - kExtractShiftAmounts below),
//     repositions the row's own 8 subphase values into their shared bit
//     range via a per-row IMMEDIATE shift (confirmed via the disassembly's
//     literal `vshl.i16 qN, qN, #14/#12/.../#2` instructions, one per row,
//     row 7 using no shift at all), and accumulates via plain vector ADD
//     instead of OR across all 8 rows - safe because each row's 2-bit field
//     lands in a disjoint bit range of the 16-bit lane (rows exactly tile
//     the 16 bits with no overlap), so ADD and OR are equivalent here with
//     no carry ever crossing a field boundary. This eliminates the
//     horizontal lane-reduce entirely (no vext/vorr fold chain) - the whole
//     8-subphase result falls out as one accumulator vector, one vst1q away
//     from `shared[]`.
//   - The portable scalar fallback is the original nested loop, used for
//     non-NEON builds (e.g. the x86_64 dev-host/clang emulator preset, which
//     has no ARM target at all).
//
// Both are byte-for-byte equivalent by construction: OR (scalar) and ADD
// (NEON, over disjoint bit ranges) are both commutative/associative, so
// reordering the reduction - row-outer/subphase-vectorized here vs. the
// scalar version's row-outer/subphase-inner - changes nothing observable.
// Verified via tools/swtcon-test/playback_kernel_bench.cpp and the
// swtcon-ab-test A/B harness (native builds are deterministic and
// self-consistent regardless of which path compiled in).
#if KERNEL_MODE == KERNEL_MODE_NEON
#include <arm_neon.h>

// Fused gather + 8x8 transpose for one 8-row group. `transitions` holds the
// group's 8 raw transition values (one 128-bit load); each is turned into a
// LUT-word address and load-duplicated STRAIGHT into a NEON register
// (vld1q_dup_u16), never materializing an 8-word array on the stack - this is
// the real leading-group loop's own structure (FUN_0004a234's .L0x3a538:
// `vmov.u16 rN, dX[lane]` / `add` / `vld1.16 {dY[],dZ[]}` per row), which
// fuses the data-dependent LUT gather into the transpose instead of the
// previous port's write-lut_words[8]-to-stack-then-vld1q_dup-it-back round
// trip.
static inline void
gather_and_transpose(uint16x8_t transitions,
                     const uint16_t* lut_data,
                     size_t lut_word_base,
                     uint16_t shared[8]) {
  // Extracts subphase b's 2-bit field from a row's LUT word via a right
  // shift of 2b (vshlq_u16's per-lane shift is signed; negative shifts
  // right) - the same constant vector for every row, matching the real
  // kernel loading it once (d8/d9 from a literal pool) and reusing it
  // across all 8 rows.
  static const int16_t kExtractShiftAmounts[8] = { 0,  -2,  -4,  -6,
                                                   -8, -10, -12, -14 };
  const int16x8_t extract_shift = vld1q_s16(kExtractShiftAmounts);
  const uint16x8_t three = vdupq_n_u16(3);
  uint16x8_t acc = vdupq_n_u16(0);

  // Manually unrolled (not a `for` loop) so the per-row reposition shift
  // below is a true compile-time immediate (vshlq_n_u16 requires one), and
  // so vgetq_lane_u16's lane index is a constant too - both match the real
  // kernel's own per-row immediate-shift/lane-extract instructions rather
  // than runtime-computed ones. Row 7 needs no reposition shift at all
  // (matching the real kernel emitting no shift for it either), so it gets
  // its own macro rather than a `shift == 0` branch - `vshlq_n_u16(x, 0)`
  // isn't guaranteed to be a valid immediate on every NEON intrinsics header.
#define SWTCON_PLAYBACK_ROW(r, shift)                                          \
  do {                                                                         \
    uint16_t t = vgetq_lane_u16(transitions, r);                               \
    uint16x8_t rep = vld1q_dup_u16(&lut_data[lut_word_base + t]);              \
    uint16x8_t extracted = vandq_u16(vshlq_u16(rep, extract_shift), three);    \
    acc = vaddq_u16(acc, vshlq_n_u16(extracted, shift));                       \
  } while (0)
#define SWTCON_PLAYBACK_ROW_NOSHIFT(r)                                         \
  do {                                                                         \
    uint16_t t = vgetq_lane_u16(transitions, r);                               \
    uint16x8_t rep = vld1q_dup_u16(&lut_data[lut_word_base + t]);              \
    uint16x8_t extracted = vandq_u16(vshlq_u16(rep, extract_shift), three);    \
    acc = vaddq_u16(acc, extracted);                                           \
  } while (0)
  SWTCON_PLAYBACK_ROW(0, 14);
  SWTCON_PLAYBACK_ROW(1, 12);
  SWTCON_PLAYBACK_ROW(2, 10);
  SWTCON_PLAYBACK_ROW(3, 8);
  SWTCON_PLAYBACK_ROW(4, 6);
  SWTCON_PLAYBACK_ROW(5, 4);
  SWTCON_PLAYBACK_ROW(6, 2);
  SWTCON_PLAYBACK_ROW_NOSHIFT(7);
#undef SWTCON_PLAYBACK_ROW
#undef SWTCON_PLAYBACK_ROW_NOSHIFT

  vst1q_u16(shared, acc);
}

// Processes every 8-row group of one column, advancing each frame slot's dest
// pointer in place: a 4-group bulk loop (the real aligned kernel's own case-8
// strategy) then a 1-group-at-a-time remainder.
//
// Bulk (FUN_0004a234's .L0x3d54c): 4 row-groups per iteration so the expensive
// per-transition NEON->ARM gather moves (vmov.u16, ~20 cyc latency on
// Cortex-A7) of 4 independent groups overlap, instead of stalling a tight
// per-group dependency chain. vld4q_u16 deinterleaves the block's 32
// contiguous transitions so val[k] lane 2g = group g's row k, lane 2g+1 = row
// k+4 (k=0..3). After the scalar LUT gather (still data-dependent - no NEON
// gather exists), each needed subphase b is extracted from all 4 groups at
// once:
//   contrib_k = (val[k] >> 2b) & 3   (even lane rows 0..3, odd rows 4..7)
//   res = (c0<<6)+(c1<<4)+(c2<<2)+c3
// packing, per group, rows 0..3 into res's even lane and rows 4..7 into its
// odd lane. vrev32 swaps the pair into little-endian low/high byte order (row
// 0 -> bit 14 ... row 7 -> bit 0, same layout as
// compute_shared_subphase_words), vmovn packs 4 groups' 16-bit results into one
// d-register.
//
// Store, aligned (Overwrite): a direct vst1_lane_u16 per group straight to its
// strided slot address - matches the real aligned kernel's own per-group
// vst1.16 {d[j]} stores, touching only the 4 target halfwords. Store, plain
// (OR): the 4 results are zero-interleaved to the even halfword positions and
// merged with vld1q/vorrq/vst1q over the block's 8 contiguous halfwords (odd
// ones in between RMW-preserved). Either way no result leaves NEON for a GPR.
//
// dest_ptrs is built here rather than passed in on purpose: taking a local
// array's address to pass it defeats GCC's scalar-replacement-of-aggregates,
// forcing all 8 pointers through the stack across the bulk loop instead of
// registers (measured ~4.5% slower on full-screen fc=8). Keeping it local -
// its address never escapes - lets them stay in registers.
template<bool Overwrite>
static inline void
process_column(void** frame_slots,
               size_t byte_off,
               int frame_count,
               int num_groups,
               const uint16_t* transitionData,
               size_t col_base,
               const uint16_t* lut_data,
               size_t lut_word_base,
               int phase_bit0) {
  // Group stride within a slot is the +4 bytes = 2 uint16_t advanced below.
  uint16_t* dest_ptrs[8];
  for (int k = 0; k < frame_count; k++)
    dest_ptrs[k] = (uint16_t*)((uint8_t*)frame_slots[k] + byte_off);

  int g = 0;
  const uint16x8_t three = vdupq_n_u16(3);
  for (; g + 4 <= num_groups; g += 4) {
    size_t base_idx = col_base + (size_t)g * 8;
    uint16_t lw[32];
    for (int i = 0; i < 32; i++)
      lw[i] = lut_data[lut_word_base + transitionData[base_idx + i]];
    uint16x8x4_t gw = vld4q_u16(lw);

    for (int k = 0; k < frame_count; k++) {
      int b = phase_bit0 + k;
      int16x8_t esh = vdupq_n_s16((int16_t)(-2 * b));
      uint16x8_t c0 = vandq_u16(vshlq_u16(gw.val[0], esh), three);
      uint16x8_t c1 = vandq_u16(vshlq_u16(gw.val[1], esh), three);
      uint16x8_t c2 = vandq_u16(vshlq_u16(gw.val[2], esh), three);
      uint16x8_t c3 = vandq_u16(vshlq_u16(gw.val[3], esh), three);
      uint16x8_t res =
        vaddq_u16(vaddq_u16(vshlq_n_u16(c0, 6), vshlq_n_u16(c1, 4)),
                  vaddq_u16(vshlq_n_u16(c2, 2), c3));
      res = vrev32q_u16(res);
      uint16x4_t vals = vreinterpret_u16_u8(vmovn_u16(res));
      uint16_t* d = dest_ptrs[k];
      if constexpr (Overwrite) {
        vst1_lane_u16(d + 0, vals, 0);
        vst1_lane_u16(d + 2, vals, 1);
        vst1_lane_u16(d + 4, vals, 2);
        vst1_lane_u16(d + 6, vals, 3);
      } else {
        uint16x4x2_t z = vzip_u16(vals, vdup_n_u16(0));
        uint16x8_t spread = vcombine_u16(z.val[0], z.val[1]);
        vst1q_u16(d, vorrq_u16(vld1q_u16(d), spread));
      }
    }
    for (int k = 0; k < frame_count; k++)
      dest_ptrs[k] += 8; // 4 groups * 2 uint16_t
  }

  for (; g < num_groups; g++) {
    size_t idx0 = col_base + (size_t)g * 8;
    uint16_t shared[8];
    uint16x8_t transitions = vld1q_u16(&transitionData[idx0]);
    gather_and_transpose(transitions, lut_data, lut_word_base, shared);
    for (int k = 0; k < frame_count; k++) {
      store_group<Overwrite>(dest_ptrs[k], shared[phase_bit0 + k]);
      dest_ptrs[k] += 2;
    }
  }
}
#elif KERNEL_MODE == KERNEL_MODE_C

static inline void
compute_shared_subphase_words(const uint16_t lut_words[8], uint16_t shared[8]) {
  for (int b = 0; b < 8; b++)
    shared[b] = 0;
  for (int r = 0; r < 8; r++) {
    uint16_t lut_word = lut_words[r];
    int shift = (7 - r) * 2;
    for (int b = 0; b < 8; b++) {
      uint16_t value = (lut_word >> (2 * b)) & 3;
      shared[b] = (uint16_t)(shared[b] | (value << shift));
    }
  }
}

// Portable scalar counterpart of the NEON process_column above (non-ARM host
// builds): one group at a time, materializing lut_words[8] for the transpose.
template<bool Overwrite>
static inline void
process_column(void** frame_slots,
               size_t byte_off,
               int frame_count,
               int num_groups,
               const uint16_t* transitionData,
               size_t col_base,
               const uint16_t* lut_data,
               size_t lut_word_base,
               int phase_bit0) {
  uint16_t* dest_ptrs[8];
  for (int k = 0; k < frame_count; k++)
    dest_ptrs[k] = (uint16_t*)((uint8_t*)frame_slots[k] + byte_off);

  for (int g = 0; g < num_groups; g++) {
    size_t idx0 = col_base + (size_t)g * 8;
    uint16_t lut_words[8];
    for (int r = 0; r < 8; r++)
      lut_words[r] = lut_data[lut_word_base + transitionData[idx0 + r]];
    uint16_t shared[8];
    compute_shared_subphase_words(lut_words, shared);
    for (int k = 0; k < frame_count; k++) {
      store_group<Overwrite>(dest_ptrs[k], shared[phase_bit0 + k]);
      dest_ptrs[k] += 2;
    }
  }
}
#endif

#if KERNEL_MODE != KERNEL_MODE_ASM

// The plain (Overwrite=false) and aligned (Overwrite=true) kernels differ only
// in how process_column stores results - see store_group.
template<bool Overwrite>
static inline void
playback_kernel_impl(void** frame_slots,
                     WorkItem* item,
                     int frame_count,
                     int chunk_index,
                     int chunk_count) {
  if (item->rectY1 < item->rectY0 || item->rectX1 < item->rectX0)
    return;

  int num_groups = ((item->rectY1 - item->rectY0) + 1) >> 3;
  if (num_groups == 0)
    return;

  int span = item->rectX1 - item->rectX0; // 0-based, inclusive
  int chunk_width = (span + 1) / chunk_count;
  int col_lo = chunk_width * chunk_index;
  int col_hi =
    (chunk_index != chunk_count - 1) ? (chunk_width - 1 + col_lo) : span;

  const auto* lut = item->lut.get();
  const uint16_t* lut_data = (const uint16_t*)lut->data;
  int mw = lut->mode_width;
  int word_idx = item->phase / 8;
  int phase_bit0 = item->phase & 7;
  // Loop-invariant across every column/group/row of this call - hoisted out
  // of the per-pixel gather below (was previously recomputed from scratch
  // 8x per group, i.e. up to num_groups*8 times per column).
  const size_t lut_word_base = (size_t)mw * mw * word_idx + word_idx;

  const uint16_t* transitionData = (const uint16_t*)item->transitionDataPtr;
  int stride = item->pixelTransitions->stride;

  for (int c = col_lo; c <= col_hi; c++) {
    int col = item->rectX0 + c;
    // transitionData is already rebased to THIS item's own rect origin (see
    // WorkItem::transitionDataPtr's comment / commit_item), so the index is
    // item-rect-relative: group g row r sits at col_base + g*8 + r - which is
    // why a group's (and a 4-group block's) rows are contiguous.
    const size_t col_base = (size_t)stride * (col - item->rectX0);

    // Group g's result lands at frame slot byte offset
    // ((col+3)*0x104 + (rectY0>>3) + g + 0x1a)*4 - a 4-byte per-group stride;
    // process_column resolves each slot's base from this and walks it.
    size_t byte_off =
      (size_t)((col + 3) * 0x104 + (item->rectY0 >> 3) + 0x1a) * 4;

    process_column<Overwrite>(frame_slots,
                              byte_off,
                              frame_count,
                              num_groups,
                              transitionData,
                              col_base,
                              lut_data,
                              lut_word_base,
                              phase_bit0);
  }
}

void
PLAYBACK_KERNEL_PLAIN_FN(void** frame_slots,
                         WorkItem* item,
                         int frame_count,
                         int chunk_index,
                         int chunk_count) {
  playback_kernel_impl<false>(
    frame_slots, item, frame_count, chunk_index, chunk_count);
}

void
PLAYBACK_KERNEL_ALIGNED_FN(void** frame_slots,
                           WorkItem* item,
                           int frame_count,
                           int chunk_index,
                           int chunk_count) {
  playback_kernel_impl<true>(
    frame_slots, item, frame_count, chunk_index, chunk_count);
}

#else

extern "C" void
playback_kernel_plain(void** frame_slots,
                      WorkItem* item,
                      int frame_count,
                      int chunk_index,
                      int chunk_count);

extern "C" void
playback_kernel_aligned(void** frame_slots,
                        WorkItem* item,
                        int frame_count,
                        int chunk_index,
                        int chunk_count);

void
playback_kernel_plain_intrinsics(void** frame_slots,
                                 WorkItem* item,
                                 int frame_count,
                                 int chunk_index,
                                 int chunk_count) {
  playback_kernel_plain(
    frame_slots, item, frame_count, chunk_index, chunk_count);
}

void
playback_kernel_aligned_intrinsics(void** frame_slots,
                                   WorkItem* item,
                                   int frame_count,
                                   int chunk_index,
                                   int chunk_count) {
  playback_kernel_aligned(
    frame_slots, item, frame_count, chunk_index, chunk_count);
}

#endif
