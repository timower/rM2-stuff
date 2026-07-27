#include "native_display.h"

#include <cstddef>
#include <cstdint>

#include "native_init.h"   // LUTEntry
#include "native_update.h" // WorkItem, RegionRows

// Native reimplementation of FUN_0004a140 (0x4a140, the "plain" playback
// kernel) - fully reversed via tools/qsgepaper-preload/playback_kernel_probe.cpp
// (isolated by-address calls against the real library with guard-paged
// buffers, cross-checked against its Ghidra decompile - see AGENTS.md). For
// each column in [rectX0,rectX1] restricted to this call's
// [chunkIndex,chunkCount) column sub-range, and each 8-row group in
// [rectY0,rectY1]:
//   - fetches each of the group's 8 rows' packed LUT word ONCE (word_idx =
//     phase/8, indexed using the row's own transition value - the raw
//     stateDataPtr u16, used DIRECTLY as the LUT's (row*mode_width+col)
//     index, not split via >>5/&0x1f - the same formula as
//     native_read_lut_packed_pixel, see WorkItem::stateDataPtr's comment);
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
//     per-column/per-group computation done once: native_advance_work_item_frames
//     always picks frameCount so phase&7+frameCount never crosses the
//     8-sub-phase boundary of a single LUT word (confirmed: case 2's
//     decompile does one shared LUT fetch, not two independent ones).
// Destination byte offset from frameSlots[k]'s base (probe-verified across
// single/multi-group rects and multi-column chunk ranges):
//   ((col+3)*0x104 + (rectY0>>3) + group + 0x1a) * 4
// - a 32-bit-word-per-column stride matching FbInitParams' xres=0x104=260.
// mode_width is read from item->lut, but the 2-bit/8-rows-per-word packing
// constants are hardcoded immediates in the real kernel too (not driven by
// lut->bit_depth at runtime) - matches native_load_waveform, which always
// produces bit_depth=2 LUTs, so this native port hardcodes the same
// assumption rather than generalizing for a case that never occurs.
//
// Split into its own translation unit (Phase 9, see AGENTS.md/CLAUDE.md) so a
// NEON-vectorized fast path can live alongside the portable scalar one behind
// an #ifdef, without dragging every other native_display.cpp dependency
// (threads, globals, ab_capture, ...) into the same file.
//
// Non-static (extern, declared in native_display.h): tools/qsgepaper-preload/
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
//   - The NEON path loads all 8 rows' packed LUT words into a single vector
//     register and, for each of the 8 output sub-phases, extracts that
//     sub-phase's 2-bit field from all 8 rows AT ONCE (variable per-lane
//     shift + mask), places each row's value at its own destination bit
//     position (another per-lane variable shift, using the fixed
//     {14,12,10,8,6,4,2,0} row->shift mapping), and OR-reduces the 8 lanes
//     down to one scalar - mirroring the real library's own strategy here
//     (per CLAUDE.md's FUN_0004a234 cases 4-8 analysis: "extract all 8
//     sub-phases from one lane-shifted vector at once"), not a literal
//     instruction-for-instruction transliteration of its compiler-generated
//     NEON (which was never fully decompiled/named - only shape-confirmed,
//     see swtcon_architecture.md §8) but the same underlying vectorization.
//   - The portable scalar fallback is the original nested loop, used for
//     non-NEON builds (e.g. the x86_64 dev-host/clang emulator preset, which
//     has no ARM target at all).
//
// Both are byte-for-byte equivalent by construction: OR is commutative and
// associative, so reordering the reduction (subphase-outer/row-vectorized
// here vs. the scalar version's row-outer/subphase-inner) changes nothing
// observable. Verified via tools/qsgepaper-preload/playback_kernel_bench.cpp
// and the swtcon-ab-test A/B harness (native builds are deterministic and
// self-consistent regardless of which path compiled in).
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define SWTCON_PLAYBACK_KERNEL_NEON 1
#include <arm_neon.h>

static inline void
compute_shared_subphase_words(const uint16_t lut_words[8], uint16_t shared[8]) {
  // Row r's 2-bit value lands at bit (7-r)*2 of its destination subphase
  // word - see the file header comment.
  static const int16_t kPlaceShiftAmounts[8] = {14, 12, 10, 8, 6, 4, 2, 0};
  const uint16x8_t words = vld1q_u16(lut_words);
  const int16x8_t place_shift = vld1q_s16(kPlaceShiftAmounts);
  const uint16x8_t three = vdupq_n_u16(3);

  for (int b = 0; b < 8; b++) {
    // vshlq_u16 takes a per-lane SIGNED shift amount; negative shifts
    // right. -2*b extracts bits [2b,2b+1] of each row's LUT word into the
    // low 2 bits of that lane, for all 8 rows in one instruction.
    const int16x8_t extract_shift = vdupq_n_s16((int16_t)(-2 * b));
    const uint16x8_t two_bits = vandq_u16(vshlq_u16(words, extract_shift), three);
    const uint16x8_t placed = vshlq_u16(two_bits, place_shift);

    // Horizontal OR-reduce the 8 lanes down to one scalar: fold the high
    // half into the low half, then fold the remaining 4/2 lanes pairwise.
    const uint16x4_t or4 = vorr_u16(vget_low_u16(placed), vget_high_u16(placed));
    const uint16x4_t or2 = vorr_u16(or4, vext_u16(or4, or4, 2));
    const uint16x4_t or1 = vorr_u16(or2, vext_u16(or2, or2, 1));
    shared[b] = vget_lane_u16(or1, 0);
  }
}
#else
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

void
native_playback_kernel_plain(void** frame_slots, WorkItem* item, int frame_count, int chunk_index,
                              int chunk_count) {
  if (item->rectY1 < item->rectY0 || item->rectX1 < item->rectX0)
    return;

  int num_groups = ((item->rectY1 - item->rectY0) + 1) >> 3;
  if (num_groups == 0)
    return;

  int span = item->rectX1 - item->rectX0;  // 0-based, inclusive
  int chunk_width = (span + 1) / chunk_count;
  int col_lo = chunk_width * chunk_index;
  int col_hi = (chunk_index != chunk_count - 1) ? (chunk_width - 1 + col_lo) : span;

  const auto* lut = (const LUTEntry*)item->lut.ptr;
  const uint16_t* lut_data = (const uint16_t*)lut->data;
  int mw = lut->mode_width;
  int word_idx = item->phase / 8;
  int phase_bit0 = item->phase & 7;
  // Loop-invariant across every column/group/row of this call - hoisted out
  // of the per-pixel gather below (was previously recomputed from scratch
  // 8x per group, i.e. up to num_groups*8 times per column).
  const size_t lut_word_base = (size_t)mw * mw * word_idx + word_idx;

  const uint16_t* state = (const uint16_t*)item->stateDataPtr;
  int stride = ((const RegionRows*)item->sp3.ptr)->stride;

  for (int c = col_lo; c <= col_hi; c++) {
    int col = item->rectX0 + c;
    // Loop-invariant across this column's groups/rows - only `row` varies
    // inside the group loop below, so this multiply need not be repeated
    // per row (was previously recomputed 8x per group).
    const size_t col_base = (size_t)stride * (col - item->rectX0);

    for (int g = 0; g < num_groups; g++) {
      int row_base = item->rectY0 + g * 8;

      // stateDataPtr (state) is already rebased to THIS item's own rect
      // origin (see WorkItem::stateDataPtr's comment / native_commit_item),
      // so the index here must be item-rect-relative, not sp3-relative -
      // indexing with (col-sp3->x0)/(row-sp3->y0) here would double-apply
      // the rebase and run off the end of the sp3 buffer for any narrowed
      // item (item rect always a subset of sp3's own, outward-8-aligned
      // rect) - confirmed via AddressSanitizer, which caught a
      // heap-buffer-overflow read here on the very first narrowed item.
      uint16_t lut_words[8];
      for (int r = 0; r < 8; r++) {
        int row = row_base + r;
        size_t idx = col_base + (size_t)(row - item->rectY0);
        uint16_t transition = state[idx];
        lut_words[r] = lut_data[lut_word_base + transition];
      }

      uint16_t shared[8];
      compute_shared_subphase_words(lut_words, shared);

      size_t byte_off = (size_t)((col + 3) * 0x104 + (item->rectY0 >> 3) + g + 0x1a) * 4;
      for (int k = 0; k < frame_count; k++) {
        auto* dest = (uint16_t*)((uint8_t*)frame_slots[k] + byte_off);
        *dest = (uint16_t)(*dest | shared[phase_bit0 + k]);
      }
    }
  }
}
