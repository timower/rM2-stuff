#include "display.h"

#include <cstddef>
#include <cstdint>

#include "init.h"   // LUTEntry
#include "update.h" // WorkItem, RegionRows

#define KERNEL_MODE_C 1
#define KERNEL_MODE_ASM 2
#define KERNEL_MODE_NEON 3

// Set by libs/swtcon/CMakeLists.txt; direct compilations that skip it (e.g.
// tools/qsgepaper-preload, arm-only) default to the real production kernel.
#ifndef SWTCON_KERNEL_MODE
#define SWTCON_KERNEL_MODE KERNEL_MODE_ASM
#endif
#define KERNEL_MODE SWTCON_KERNEL_MODE

#if KERNEL_MODE == KERNEL_MODE_NEON && !defined(__ARM_NEON) &&                 \
  !defined(__ARM_NEON__)
#error "Unsupported kernel mode"
#endif

// Native reimplementation of FUN_0004a140 (0x4a140, the "plain" playback
// kernel) - fully reversed via
// tools/qsgepaper-preload/playback_kernel_probe.cpp (isolated by-address calls
// against the real library with guard-paged buffers, cross-checked against its
// Ghidra decompile - see AGENTS.md). For each column in [rectX0,rectX1]
// restricted to this call's [chunkIndex,chunkCount) column sub-range, and each
// 8-row group in [rectY0,rectY1]:
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
// Non-static (extern, declared in display.h): tools/qsgepaper-preload/
// playback_kernel_bench.cpp calls this directly to isolate the compute cost
// of the real, shipped kernel from the threading/dispatch machinery around
// it - real hardware confirmed this is currently much slower than the
// library's NEON-hand-tuned equivalent (see that file for the benchmark).

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
// Verified via tools/qsgepaper-preload/playback_kernel_bench.cpp and the
// swtcon-ab-test A/B harness (native builds are deterministic and
// self-consistent regardless of which path compiled in).
#if KERNEL_MODE == KERNEL_MODE_NEON
#define SWTCON_PLAYBACK_KERNEL_NEON 1
#include <arm_neon.h>

static inline void
compute_shared_subphase_words(const uint16_t lut_words[8], uint16_t shared[8]) {
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
  // below is a true compile-time immediate (vshlq_n_u16 requires one),
  // matching the real kernel's own per-row immediate-shift instructions
  // rather than a runtime-computed shift amount. Row 7 needs no reposition
  // shift at all (matching the real kernel emitting no shift instruction
  // for it either), so it gets its own macro rather than a `shift == 0`
  // branch inside a single one - `vshlq_n_u16(x, 0)` isn't guaranteed to be
  // a valid immediate on every NEON intrinics header.
#define SWTCON_PLAYBACK_ROW(r, shift)                                          \
  do {                                                                         \
    uint16x8_t rep = vld1q_dup_u16(&lut_words[r]);                             \
    uint16x8_t extracted = vandq_u16(vshlq_u16(rep, extract_shift), three);    \
    acc = vaddq_u16(acc, vshlq_n_u16(extracted, shift));                       \
  } while (0)
#define SWTCON_PLAYBACK_ROW_NOSHIFT(r)                                         \
  do {                                                                         \
    uint16x8_t rep = vld1q_dup_u16(&lut_words[r]);                             \
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
#elif KERNEL_MODE == KERNEL_MODE_C
#define SWTCON_PLAYBACK_KERNEL_NEON 0

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
#endif

#if KERNEL_MODE != KERNEL_MODE_ASM

void
playback_kernel_plain_intrinsics(void** frame_slots,
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
    // Loop-invariant across this column's groups/rows - only `row` varies
    // inside the group loop below, so this multiply need not be repeated
    // per row (was previously recomputed 8x per group).
    const size_t col_base = (size_t)stride * (col - item->rectX0);

    // byte_off(g) = ((col+3)*0x104 + (rectY0>>3) + g + 0x1a) * 4 is an
    // arithmetic sequence in g with a constant stride of 4 bytes - the real
    // library precomputes this whole sequence for a column in one pass
    // rather than re-deriving it via multiplication per group (see
    // AGENTS.md's Phase 9 entry on FUN_0004a140's case-8 handler); a running
    // accumulator gets the same effect without needing a scratch array.
    size_t byte_off =
      (size_t)((col + 3) * 0x104 + (item->rectY0 >> 3) + 0x1a) * 4;

    for (int g = 0; g < num_groups; g++, byte_off += 4) {
      int row_base = item->rectY0 + g * 8;

      // transitionDataPtr (transitionData) is already rebased to THIS item's
      // own rect origin (see WorkItem::transitionDataPtr's comment /
      // commit_item), so the index here must be item-rect-relative,
      // not pixelTransitions-relative - indexing with
      // (col-pixelTransitions->x0)/(row-pixelTransitions->y0) here would
      // double-apply the rebase and run off the end of the pixelTransitions
      // buffer for any narrowed item (item rect always a subset of
      // pixelTransitions' own, outward-8-aligned rect) - confirmed via
      // AddressSanitizer, which caught a heap-buffer-overflow read here on
      // the very first narrowed item.
      uint16_t lut_words[8];
#if SWTCON_PLAYBACK_KERNEL_NEON
      // A group's 8 rows are contiguous in `transitionData` (idx increases
      // by exactly 1 per row - see the comment above), so all 8 transition
      // values can be gathered with a single 128-bit load instead of 8
      // scalar ones - confirmed directly in the real aligned kernel's own
      // case-8 handler (FUN_0004a234, 0x4d1c0: `vld1.16 {d6,d7},[r3]` loads
      // exactly one group's 8 transitions at once, vs. this port's previous
      // 8 separate `ldrh`-equivalent scalar reads). The subsequent per-lane
      // extract (vgetq_lane_u16) is still scalar because the LUT lookup
      // itself is a genuine data-dependent gather - this target's NEON has
      // no gather instruction, and the real kernel doesn't have one either
      // (see AGENTS.md's Phase 9 entry).
      size_t idx0 = col_base + (size_t)(row_base - item->rectY0);
      uint16x8_t transitions = vld1q_u16(&transitionData[idx0]);
      lut_words[0] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 0)];
      lut_words[1] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 1)];
      lut_words[2] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 2)];
      lut_words[3] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 3)];
      lut_words[4] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 4)];
      lut_words[5] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 5)];
      lut_words[6] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 6)];
      lut_words[7] = lut_data[lut_word_base + vgetq_lane_u16(transitions, 7)];
#else
      for (int r = 0; r < 8; r++) {
        int row = row_base + r;
        size_t idx = col_base + (size_t)(row - item->rectY0);
        uint16_t transition = transitionData[idx];
        lut_words[r] = lut_data[lut_word_base + transition];
      }
#endif

      uint16_t shared[8];
      compute_shared_subphase_words(lut_words, shared);

      for (int k = 0; k < frame_count; k++) {
        auto* dest = (uint16_t*)((uint8_t*)frame_slots[k] + byte_off);
        *dest = (uint16_t)(*dest | shared[phase_bit0 + k]);
      }
    }
  }
}
void
playback_kernel_aligned_intrinsics(void** frame_slots,
                               WorkItem* item,
                               int frame_count,
                               int chunk_index,
                               int chunk_count) {
  playback_kernel_plain_intrinsics(
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
