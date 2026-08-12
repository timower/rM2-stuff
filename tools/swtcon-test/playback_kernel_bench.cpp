// Microbenchmark comparing every "pure" (no dlopen, no shared globals)
// worker-side playback kernel implementation in this codebase, on the same
// synthetic WorkItem/transitions/LUT input:
//   - scalar: the portable KERNEL_MODE_C fallback in
//     playback_kernel_intrinsics.cpp (playback_kernel_plain_intrinsics_c).
//   - neon: the from-scratch KERNEL_MODE_NEON intrinsics port in the same
//     file (playback_kernel_plain_intrinsics_neon) - no longer in the
//     production dispatch path (see doc/swtcon_porting.md Phase 9's fifth
//     pass), kept here as the thing an optimization pass on the intrinsics
//     port should be measured against.
//   - asm-plain/asm-aligned: the raw ARM/NEON assembly port
//     (playback_kernel.s/playback_kernel_aligned.s) - a line-for-line
//     transliteration of the real library's own FUN_0004a140/FUN_0004a234,
//     and the actual production kernel today. This is the target the neon
//     port needs to close the gap with.
// See libs/swtcon/playback_kernel_intrinsics.cpp's SWTCON_KERNEL_SYMBOL_SUFFIX
// comment and tools/swtcon-test/CMakeLists.txt for how all three get linked
// into one binary (the same source file compiled three times, once per
// KERNEL_MODE, under a distinct symbol each time).
//
// Previously this tool instead dlopen'd the real on-device
// libqsgepaper.so and called its FUN_0004a140/FUN_0004a234 by address (the
// same technique as playback_kernel_probe.cpp) - useful while that library
// was still the ground truth to port against, but no longer necessary now
// that the ASM port is a confirmed byte-for-byte + real-hardware-verified
// transliteration of it (see doc/swtcon_porting.md) and production no longer
// calls the library at all. Dropping the dlopen dependency also means this
// tool no longer needs the real device rootfs to run anywhere useful.
//
// Caveat: absolute ns/call numbers are only meaningful relative to each
// other WITHIN one run of this tool on one machine - comparing an emulator
// run's numbers to a hardware run's numbers is not valid, but the
// neon-vs-asm RATIO computed in each run should be roughly comparable across
// environments.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "display.h" // WorkItem via update.h
#include "init.h"    // LUTEntry
#include "update.h"  // WorkItem, RegionRows

// This bench doesn't link swtcon.cpp (see CMakeLists.txt) - it never uses
// SWTCON_LIBIMPL library mode, so a permanently-false stub plus null
// function-pointer globals are enough to satisfy update.cpp's link
// requirements.
bool
swtcon_lib_impl_enabled() {
  return false;
}
void (*qsgepaper_lock)() = nullptr;
void (*qsgepaper_update)(update_data*) = nullptr;
void (*qsgepaper_unlock_post)() = nullptr;
void (*qsgepaper_wait)() = nullptr;

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
playback_kernel_plain_intrinsics_c(void** frame_slots,
                                   WorkItem* item,
                                   int frame_count,
                                   int chunk_index,
                                   int chunk_count);
void
playback_kernel_plain_intrinsics_neon(void** frame_slots,
                                      WorkItem* item,
                                      int frame_count,
                                      int chunk_index,
                                      int chunk_count);
// The overwrite (non-OR) intrinsics variants, mirroring asm-aligned. On the
// cleared slots this bench uses they produce identical output to the plain
// variants; they exist to compare the store strategy (see the kernel's
// playback_kernel_impl<Overwrite> template).
void
playback_kernel_aligned_intrinsics_c(void** frame_slots,
                                     WorkItem* item,
                                     int frame_count,
                                     int chunk_index,
                                     int chunk_count);
void
playback_kernel_aligned_intrinsics_neon(void** frame_slots,
                                        WorkItem* item,
                                        int frame_count,
                                        int chunk_index,
                                        int chunk_count);

// One full hardware frame slot, bytes - see playback_kernel_probe.cpp's
// comment (matches init_lut's 0x165800-byte LUT blob size, not a
// coincidence).
constexpr size_t kFrameSlotBytes = 0x165800;

// kScreenWidth/kScreenHeight (the largest rect any single WorkItem can ever
// cover - a full-panel HQ refresh, swtcon_architecture.md §6.2 step 2) and
// kPanelFrameTickUs both live in qsgepaper_globals.h now, shared with
// display.cpp's display_thread_func pacing-target formula (which
// used to re-hardcode its own copies of the latter). This comment on
// kPanelFrameTickUs is kept here since this is still the file that explains
// what the constant is FOR in benchmark terms: not this kernel's own budget
// (the whole pipeline - dispatch, both kernels, panning - shares it across
// however many in-flight items there are), but the only concrete real-time
// deadline in this codebase to size "is this fast enough" against.

// Number of 16-bit words a LUTEntry needs for mode_width x mode_width packed
// pixels across phases 0..lutWidth-1 - same formula as
// playback_kernel_probe.cpp's lut_data_words.
static size_t
lut_data_words(int mode_width, int bit_depth, int lut_width) {
  int pixels_per_word = 16 / bit_depth;
  int max_word_idx = (lut_width - 1) / pixels_per_word;
  return (size_t)(mode_width * mode_width + 1) * (max_word_idx + 1);
}

// A self-contained synthetic WorkItem - only the fields
// playback_kernel_plain_intrinsics actually reads (rect, phase, lut,
// pixelTransitions.stride, transitionDataPtr; see its definition in
// display.cpp) need to be valid, so unlike playback_kernel_probe.cpp's
// build_item this needs no WorkItemNode/regionRows/intList scaffolding.
// LUTEntry (init.h) already frees `data` in its own destructor
// (init.cpp: `if (data) free(data)`) - do NOT also free it here. This
// is the exact double-free gotcha playback_kernel_probe.cpp's build_item hit
// (see its free_item comment): a LUTEntry embedded by value gets its
// destructor run automatically at scope exit, so an extra explicit free
// alongside it is a double free, not a safety net.
struct BenchItem {
  WorkItem item{};
  RegionRows transitionsRr{};
  std::vector<uint16_t> transitions;
  LUTEntry lut{};
};

static void
build_item(BenchItem& bi,
           int y0,
           int x0,
           int y1,
           int x1,
           int mode_width,
           int bit_depth,
           int lut_width,
           uint32_t seed) {
  int rows = y1 - y0 + 1;
  int cols = x1 - x0 + 1;
  int strideRows = (rows + 15) & ~15; // same rounding RegionRows uses elsewhere

  uint32_t s = seed;
  auto next_rand = [&]() {
    s = s * 1103515245u + 12345u;
    return (s >> 16) & 0x7fff;
  };

  // Varied, not uniform: the kernel's LUT lookup index depends on the
  // per-pixel transition value, so a uniform fill would hit one LUT cache
  // line over and over - not representative of a real, non-uniform screen
  // update.
  bi.transitions.assign((size_t)strideRows * cols, 0);
  for (auto& v : bi.transitions)
    v = (uint16_t)((next_rand() % mode_width) * mode_width +
                   (next_rand() % mode_width));

  bi.transitionsRr.dataPtr = (uint8_t*)bi.transitions.data();
  bi.transitionsRr.y0 = y0;
  bi.transitionsRr.x0 = x0;
  bi.transitionsRr.y1 = y1;
  bi.transitionsRr.x1 = x1;
  bi.transitionsRr.stride = strideRows;
  bi.transitionsRr.size = strideRows * cols * (int)sizeof(uint16_t);

  size_t lut_words = lut_data_words(mode_width, bit_depth, lut_width);
  bi.lut.size_kb =
    lut_width; // size_kb doubles as phase count, see load_waveform
  bi.lut.mode_width = mode_width;
  bi.lut.temperature = 0;
  bi.lut.bit_depth = bit_depth;
  bi.lut.data = malloc(lut_words * sizeof(uint16_t));
  auto* ld = (uint16_t*)bi.lut.data;
  for (size_t i = 0; i < lut_words; i++)
    ld[i] = (uint16_t)next_rand();

  bi.item.rectY0 = y0;
  bi.item.rectX0 = x0;
  bi.item.rectY1 = y1;
  bi.item.rectX1 = x1;
  bi.item.phase = 0; // 8-aligned: lets the aligned kernel path be exercised too
  bi.item.lutWidthMinus1 = (int16_t)(lut_width - 1);
  bi.item.lut = non_owning_sp(&bi.lut);
  bi.item.pixelTransitions = non_owning_sp(&bi.transitionsRr);
  bi.item.transitionDataPtr =
    bi.transitions.data(); // no rebase needed: already item-rect-relative
}

struct Result {
  long iterations = 0;
  double ns_per_call = 0;
};

// Runs `fn` repeatedly until both a minimum iteration count and a minimum
// wall-clock duration are satisfied, after a short warmup. Fast scenarios
// hit the time floor; slow scenarios (full-screen refreshes) hit the
// iteration floor - either way this settles quickly without needing
// per-scenario tuning.
template<typename Fn>
static Result
run_bench(Fn&& fn, long min_iterations = 20, double min_ms = 300.0) {
  using Clock = std::chrono::steady_clock;
  for (int i = 0; i < 5; i++)
    fn();

  Result r;
  auto start = Clock::now();
  double elapsed_ms = 0;
  do {
    fn();
    r.iterations++;
    elapsed_ms =
      std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  } while ((r.iterations < min_iterations || elapsed_ms < min_ms) &&
           r.iterations < 2'000'000);
  r.ns_per_call = elapsed_ms * 1e6 / (double)r.iterations;
  return r;
}

struct Scenario {
  const char* name;
  int y0, x0, y1, x1;
  int frame_count;
};

static void
print_row(const char* label, const BenchItem& bi, const Result& r) {
  int cols = bi.item.rectX1 - bi.item.rectX0 + 1;
  int rows = bi.item.rectY1 - bi.item.rectY0 + 1;
  double mpix_per_sec = (double)cols * rows / (r.ns_per_call * 1e-9) / 1e6;
  double pct_of_tick = (r.ns_per_call / 1000.0) / kPanelFrameTickUs * 100.0;
  printf("    %-10s %10.1f ns/call  %8.1f Mpix/s  %6.2f%% of a %.2fms panel "
         "tick  (%ld iters)\n",
         label,
         r.ns_per_call,
         mpix_per_sec,
         pct_of_tick,
         kPanelFrameTickUs / 1000.0,
         r.iterations);
}

// Runs `fn` into a freshly-cleared copy of the 8 frame slots and returns them,
// so two kernels' full output can be compared byte-for-byte. The kernels OR
// (accumulate) into their target, so clearing first makes the result a pure
// function of the input - a mismatch is a real behavioural divergence.
using Slots = std::vector<std::vector<uint8_t>>;
template<typename Fn>
static Slots
run_into_fresh_slots(Fn&& fn) {
  Slots slots(8);
  std::vector<void*> ptrs(8);
  for (int i = 0; i < 8; i++) {
    slots[i].assign(kFrameSlotBytes, 0);
    ptrs[i] = slots[i].data();
  }
  fn(ptrs.data());
  return slots;
}

// Cross-checks every kernel against asm-aligned (the shipped production
// kernel, ground truth) on one scenario - the bench otherwise only measures
// speed and would happily report a fast but wrong kernel. Returns false on any
// mismatch.
static bool
verify_matches_reference(BenchItem& bi, int frame_count) {
  auto ref = run_into_fresh_slots([&](void** fs) {
    playback_kernel_aligned(fs, &bi.item, frame_count, 0, 1);
  });
  struct Candidate {
    const char* name;
    void (*fn)(void**, WorkItem*, int, int, int);
  };
  Candidate candidates[] = {
    { "scalar", playback_kernel_plain_intrinsics_c },
    { "neon", playback_kernel_plain_intrinsics_neon },
    { "scalar-aligned", playback_kernel_aligned_intrinsics_c },
    { "neon-aligned", playback_kernel_aligned_intrinsics_neon },
    { "asm-plain", playback_kernel_plain },
  };
  bool ok = true;
  for (auto& c : candidates) {
    auto got = run_into_fresh_slots(
      [&](void** fs) { c.fn(fs, &bi.item, frame_count, 0, 1); });
    for (int i = 0; i < 8; i++) {
      if (memcmp(got[i].data(), ref[i].data(), kFrameSlotBytes) != 0) {
        printf(
          "    !! %s MISMATCH vs asm-aligned in frame slot %d\n", c.name, i);
        ok = false;
        break;
      }
    }
  }
  if (ok)
    printf("    (all kernels match asm-aligned)\n");
  return ok;
}

static void
print_ratio(const char* subject,
            const Result& r,
            const char* baseline_label,
            const Result& baseline) {
  printf("    -> %s is %.2fx the %s's time\n",
         subject,
         r.ns_per_call / baseline.ns_per_call,
         baseline_label);
}

int
main() {
  setvbuf(stdout, nullptr, _IONBF, 0);

  void** frame_slots = (void**)malloc(8 * sizeof(void*));
  for (int i = 0; i < 8; i++)
    frame_slots[i] = aligned_alloc(64, kFrameSlotBytes);
  auto clear_slots = [&] {
    for (int i = 0; i < 8; i++)
      memset(frame_slots[i], 0, kFrameSlotBytes);
  };

  // Rect shapes spanning the real range of update sizes - from a single
  // 8-row glyph strip up to a full-panel HQ refresh (1404x1872,
  // 234 8-row groups) - crossed with the two frameCount extremes:
  // frameCount=8 is what real traffic hits almost exclusively (see
  // doc/swtcon_porting.md's Phase 9 entry), frameCount=1 is the opposite,
  // most-general-case end of the (phase&7)+k loop.
  Scenario scenarios[] = {
    { "tiny 32x8 (1 group)", 0, 0, 7, 31, 8 },
    { "text-row 256x16 (2 groups)", 0, 0, 15, 255, 8 },
    { "menu 400x128 (16 groups)", 0, 0, 127, 399, 8 },
    { "half-screen 700x936 (117 groups)", 0, 0, 935, 699, 8 },
    { "full-screen 1404x1872 (234 groups) fc=8",
      0,
      0,
      kScreenHeight - 1,
      kScreenWidth - 1,
      8 },
    { "full-screen 1404x1872 (234 groups) fc=1",
      0,
      0,
      kScreenHeight - 1,
      kScreenWidth - 1,
      1 },
  };

  for (auto& sc : scenarios) {
    printf("=== %s, frameCount=%d ===\n", sc.name, sc.frame_count);
    BenchItem bi;
    build_item(bi,
               sc.y0,
               sc.x0,
               sc.y1,
               sc.x1,
               /*mode_width=*/32,
               /*bit_depth=*/2,
               /*lut_width=*/16,
               /*seed=*/0xC0FFEEu + sc.y1);

    verify_matches_reference(bi, sc.frame_count);

    clear_slots();
    auto scalar_result = run_bench([&] {
      playback_kernel_plain_intrinsics_c(frame_slots,
                                         &bi.item,
                                         sc.frame_count,
                                         /*chunk_index=*/0,
                                         /*chunk_count=*/1);
    });
    print_row("scalar", bi, scalar_result);

    clear_slots();
    auto neon_result = run_bench([&] {
      playback_kernel_plain_intrinsics_neon(frame_slots,
                                            &bi.item,
                                            sc.frame_count,
                                            /*chunk_index=*/0,
                                            /*chunk_count=*/1);
    });
    print_row("neon", bi, neon_result);
    print_ratio("neon", neon_result, "scalar kernel", scalar_result);

    clear_slots();
    auto neon_aligned_result = run_bench([&] {
      playback_kernel_aligned_intrinsics_neon(frame_slots,
                                              &bi.item,
                                              sc.frame_count,
                                              /*chunk_index=*/0,
                                              /*chunk_count=*/1);
    });
    print_row("neon-align", bi, neon_aligned_result);

    clear_slots();
    auto asm_plain_result = run_bench([&] {
      playback_kernel_plain(frame_slots,
                            &bi.item,
                            sc.frame_count,
                            /*chunk_index=*/0,
                            /*chunk_count=*/1);
    });
    print_row("asm-plain", bi, asm_plain_result);
    print_ratio("neon", neon_result, "asm-plain kernel", asm_plain_result);

    // The aligned kernel is only ever selected at an 8-aligned phase
    // (advance_work_item_frames's selection rule) - this synthetic item's
    // phase=0 satisfies that.
    clear_slots();
    auto asm_aligned_result = run_bench([&] {
      playback_kernel_aligned(frame_slots,
                              &bi.item,
                              sc.frame_count,
                              /*chunk_index=*/0,
                              /*chunk_count=*/1);
    });
    print_row("asm-aligned", bi, asm_aligned_result);
    print_ratio("neon", neon_result, "asm-aligned kernel", asm_aligned_result);
    print_ratio("neon-align",
                neon_aligned_result,
                "asm-aligned kernel",
                asm_aligned_result);

    printf("\n");
  }

  printf("Note: absolute ns/call numbers are only meaningful within this "
         "single run/machine - the\n"
         "neon-vs-asm ratios are the portable signal. The %.2fms figure is "
         "the whole display\n"
         "pipeline's per-frame-tick pacing budget (display.cpp), not this "
         "kernel's own budget\n"
         "in isolation - shown for scale, not as a hard per-call target.\n",
         kPanelFrameTickUs / 1000.0);

  return 0;
}
