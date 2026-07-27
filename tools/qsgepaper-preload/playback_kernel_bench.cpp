// Microbenchmark for native_playback_kernel_plain (native_display.cpp) - the
// native port of the worker-side playback kernels FUN_0004a140/FUN_0004a234
// (see AGENTS.md). Real-hardware testing found the ported kernel noticeably
// slower than the library it replaces, which implements it as hand-tuned
// NEON - this tool isolates just the kernel's own compute cost (no
// threading/dispatch/list-bookkeeping around it) so an optimization pass has
// a concrete before/after number, rather than only "the panel feels slower".
//
// Two things this tool measures, both against the same synthetic WorkItem/
// state/LUT input:
//   1. native_playback_kernel_plain's wall-clock cost across a matrix of
//      rect sizes and frameCounts representative of real update traffic
//      (see swtcon_architecture.md §6.4/§6.5 for where these shapes and the
//      dominant frameCount=8 case come from).
//   2. The same input run through the real library's FUN_0004a140 and
//      FUN_0004a234 (dlopen'd by address, same technique as
//      playback_kernel_probe.cpp) - the NEON reference this port needs to
//      close the gap with. Optional: skipped with --no-lib, or automatically
//      if libqsgepaper.so isn't found (useful once Phase 8 drops the
//      production dlopen entirely).
//
// Caveat: absolute ns/call numbers are only meaningful relative to each
// other WITHIN one run of this tool on one machine - comparing an emulator
// run's numbers to a hardware run's numbers is not valid, but the
// native-vs-library RATIO computed in each run should be roughly comparable
// across environments, which is why running this on the emulator first
// (per the user's request) is a reasonable way to gauge how much NEON work
// is worth it before burning a hardware round-trip.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <dlfcn.h>
#include <string>
#include <ucontext.h>
#include <vector>

#include "native_display.h" // native_playback_kernel_plain
#include "native_init.h"    // LUTEntry
#include "native_update.h"  // WorkItem, RegionRows

#define INSTANCE_ADDR 0x35de0
#define PLAIN_PLAYBACK_KERNEL_ADDR 0x4a140
// FUN_0004a234 was originally called "overlap-aware" (it's selected using
// the item's overlap-dependency intList), but that's backwards: it only
// fires when there's NO active overlap dependency left AND phase is
// 8-aligned - it's an alignment fast path, not an overlap-driven one. See
// native_display.cpp's native_dispatch_aligned_kernel for the full story.
#define ALIGNED_PLAYBACK_KERNEL_ADDR 0x4a234

// One full hardware frame slot, bytes - see playback_kernel_probe.cpp's
// comment (matches native_init_lut's 0x165800-byte LUT blob size, not a
// coincidence).
constexpr size_t kFrameSlotBytes = 0x165800;

// SCREEN_WIDTH/SCREEN_HEIGHT (swtcon_architecture.md §6.2 step 2) - the
// largest rect any single WorkItem can ever cover (a full-panel HQ refresh).
constexpr int kScreenWidth = 1404;
constexpr int kScreenHeight = 1872;

// The panel's own per-frame-tick pacing period, in microseconds - the two
// pacing-target constants (11761/23523 - one and two ticks) in
// native_display.cpp's display_thread_func port. Not this kernel's own
// budget (the whole pipeline - dispatch, both kernels, panning - shares it
// across however many in-flight items there are), but the only concrete
// real-time deadline in this codebase to size "is this fast enough" against.
constexpr double kPanelFrameTickUs = 11761.0;

typedef void (*PlaybackKernelFn)(void**, WorkItem*, int, int, int);

static uintptr_t g_runtime_offset = 0;
uintptr_t
swtcon_runtime_offset() {
  return g_runtime_offset;
}

static void
segv_handler(int sig, siginfo_t* si, void* ucv) {
  auto* uc = (ucontext_t*)ucv;
  unsigned long pc = uc->uc_mcontext.arm_pc;
  unsigned long lr = uc->uc_mcontext.arm_lr;
  fprintf(stderr, "\n*** SIGSEGV: fault_addr=%p pc=0x%lx lr=0x%lx\n", si->si_addr, pc, lr);
  fprintf(stderr, "*** ghidra: pc=0x%lx lr=0x%lx (runtime_offset=0x%lx)\n", pc - g_runtime_offset,
          lr - g_runtime_offset, (unsigned long)g_runtime_offset);
  fflush(stderr);
  signal(sig, SIG_DFL);
  raise(sig);
}

// Best-effort: unlike the correctness probes, a benchmark tool shouldn't
// hard-require the library (Phase 8 plans to drop the production dlopen
// entirely, and this tool should still be useful for pure native-vs-native
// A/B optimization passes afterward).
static bool
try_load_library(uintptr_t* offset_out) {
  void* handle = dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
  if (!handle)
    handle = dlopen("./libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    fprintf(stderr, "note: libqsgepaper.so not found (%s) - skipping library comparison\n", dlerror());
    return false;
  }
  void* instance_func = dlsym(handle, "_ZN13EPFramebuffer8instanceEv");
  if (!instance_func) {
    fprintf(stderr, "note: known export symbol not found (%s) - skipping library comparison\n", dlerror());
    return false;
  }
  *offset_out = (uintptr_t)instance_func - INSTANCE_ADDR;
  return true;
}

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
// native_playback_kernel_plain actually reads (rect, phase, lut, sp3.stride,
// stateDataPtr; see its definition in native_display.cpp) need to be valid,
// so unlike playback_kernel_probe.cpp's build_item this needs no
// WorkItemNode/regionRows/intList scaffolding.
// LUTEntry (native_init.h) already frees `data` in its own destructor
// (native_init.cpp: `if (data) free(data)`) - do NOT also free it here. This
// is the exact double-free gotcha playback_kernel_probe.cpp's build_item hit
// (see its free_item comment): a LUTEntry embedded by value gets its
// destructor run automatically at scope exit, so an extra explicit free
// alongside it is a double free, not a safety net.
struct BenchItem {
  WorkItem item{};
  RegionRows sp3rr{};
  std::vector<uint16_t> state;
  LUTEntry lut{};
};

static void
build_item(BenchItem& bi, int y0, int x0, int y1, int x1, int mode_width, int bit_depth, int lut_width,
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
  // per-pixel state value, so a uniform fill would hit one LUT cache line
  // over and over - not representative of a real, non-uniform screen update.
  bi.state.assign((size_t)strideRows * cols, 0);
  for (auto& v : bi.state)
    v = (uint16_t)((next_rand() % mode_width) * mode_width + (next_rand() % mode_width));

  bi.sp3rr.dataPtr = (uint8_t*)bi.state.data();
  bi.sp3rr.y0 = y0;
  bi.sp3rr.x0 = x0;
  bi.sp3rr.y1 = y1;
  bi.sp3rr.x1 = x1;
  bi.sp3rr.stride = strideRows;
  bi.sp3rr.size = strideRows * cols * (int)sizeof(uint16_t);

  size_t lut_words = lut_data_words(mode_width, bit_depth, lut_width);
  bi.lut.size_kb = lut_width; // size_kb doubles as phase count, see native_load_waveform
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
  bi.item.lut.ptr = &bi.lut;
  bi.item.sp3.ptr = &bi.sp3rr;
  bi.item.stateDataPtr = bi.state.data(); // no rebase needed: state is item-rect-relative already
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
template <typename Fn>
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
    elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  } while ((r.iterations < min_iterations || elapsed_ms < min_ms) && r.iterations < 2'000'000);
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
  printf("    %-10s %10.1f ns/call  %8.1f Mpix/s  %6.2f%% of a %.2fms panel tick  (%ld iters)\n", label,
         r.ns_per_call, mpix_per_sec, pct_of_tick, kPanelFrameTickUs / 1000.0, r.iterations);
}

int
main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  bool want_lib = true;
  for (int i = 1; i < argc; i++)
    if (std::string(argv[i]) == "--no-lib")
      want_lib = false;

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = segv_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, nullptr);

  bool have_lib = want_lib && try_load_library(&g_runtime_offset);
  PlaybackKernelFn lib_plain = nullptr;
  PlaybackKernelFn lib_aligned = nullptr;
  if (have_lib) {
    lib_plain = (PlaybackKernelFn)(g_runtime_offset + PLAIN_PLAYBACK_KERNEL_ADDR);
    lib_aligned = (PlaybackKernelFn)(g_runtime_offset + ALIGNED_PLAYBACK_KERNEL_ADDR);
    printf("Library loaded, runtime_offset=0x%lx - will benchmark FUN_0004a140/FUN_0004a234 too\n\n",
           (unsigned long)g_runtime_offset);
  } else {
    printf("Running native-only (no library comparison)\n\n");
  }

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
  // frameCount=8 is what the ab-test's real traffic hits almost exclusively
  // (see AGENTS.md's FUN_0004a140 entry), frameCount=1 is the opposite,
  // most-general-case end of the (phase&7)+k loop.
  Scenario scenarios[] = {
    { "tiny 32x8 (1 group)", 0, 0, 7, 31, 8 },
    { "text-row 256x16 (2 groups)", 0, 0, 15, 255, 8 },
    { "menu 400x128 (16 groups)", 0, 0, 127, 399, 8 },
    { "half-screen 700x936 (117 groups)", 0, 0, 935, 699, 8 },
    { "full-screen 1404x1872 (234 groups) fc=8", 0, 0, kScreenHeight - 1, kScreenWidth - 1, 8 },
    { "full-screen 1404x1872 (234 groups) fc=1", 0, 0, kScreenHeight - 1, kScreenWidth - 1, 1 },
  };

  for (auto& sc : scenarios) {
    printf("=== %s, frameCount=%d ===\n", sc.name, sc.frame_count);
    BenchItem bi;
    build_item(bi, sc.y0, sc.x0, sc.y1, sc.x1, /*mode_width=*/32, /*bit_depth=*/2, /*lut_width=*/16,
               /*seed=*/0xC0FFEEu + sc.y1);

    clear_slots();
    auto native_result = run_bench([&] {
      native_playback_kernel_plain(frame_slots, &bi.item, sc.frame_count, /*chunk_index=*/0,
                                    /*chunk_count=*/1);
    });
    print_row("native", bi, native_result);

    if (have_lib) {
      clear_slots();
      auto lib_plain_result = run_bench([&] {
        lib_plain(frame_slots, &bi.item, sc.frame_count, /*chunk_index=*/0, /*chunk_count=*/1);
      });
      print_row("lib-plain", bi, lib_plain_result);
      printf("    -> native is %.2fx the library plain kernel's time\n",
             native_result.ns_per_call / lib_plain_result.ns_per_call);

      // The aligned kernel is only ever selected at an 8-aligned
      // phase (advance_work_item_frames's selection rule, see AGENTS.md) -
      // this synthetic item's phase=0 satisfies that.
      clear_slots();
      auto lib_aligned_result = run_bench([&] {
        lib_aligned(frame_slots, &bi.item, sc.frame_count, /*chunk_index=*/0, /*chunk_count=*/1);
      });
      print_row("lib-aligned", bi, lib_aligned_result);
      printf("    -> native is %.2fx the library aligned kernel's time\n",
             native_result.ns_per_call / lib_aligned_result.ns_per_call);
    }
    printf("\n");
  }

  printf(
    "Note: absolute ns/call numbers are only meaningful within this single run/machine - the\n"
    "native-vs-library ratios are the portable signal. The %.2fms figure is the whole display\n"
    "pipeline's per-frame-tick pacing budget (native_display.cpp), not this kernel's own budget\n"
    "in isolation - shown for scale, not as a hard per-call target.\n",
    kPanelFrameTickUs / 1000.0);

  return 0;
}
