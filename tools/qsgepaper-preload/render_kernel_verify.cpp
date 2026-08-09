// A/B verification tool for lib_render_update_kernel (0x4e7b8), still-library
// code that lib_render_update_kernel is not yet native for (AGENTS.md Phase 6).
// This does NOT reimplement the kernel's addressing/rotation logic - it
// calls the real library function directly (with the same {item, dataBuffer,
// backBuffer, chunkIndex=0, chunkCount=1} calling convention
// dispatch_update_regions itself uses for a single-threaded dispatch) and
// checks its output against the per-pixel formulas derived from disassembly
// (see swtcon_architecture.md §5.2), without needing to know which output
// byte corresponds to which input pixel.
//
// The trick: for a rect exactly 128x128 pixels, the kernel's two &0x7f loop
// counters each sweep a full 0..127 residue class exactly once (regardless
// of the rect's absolute screen position, and regardless of the 180 deg
// rotation/chunking logic we haven't reversed), so every (row,col) gamma
// table cell get visited *exactly once*, just in some unknown order. Holding
// the source pixel value uniform across the whole rect turns "byte-for-byte
// positional match" into "sorted multiset match" - which we CAN check
// without knowing the permutation.
#include <algorithm>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <ucontext.h>
#include <vector>

#include "init.h"
#include "update.h"

// Same symbol-relocation trick as swtcon.cpp's load_lib(), duplicated here
// so this tool doesn't need to run the (hardware-dependent) full swtcon_init.
#define INSTANCE_ADDR 0x35de0
#define RENDER_UPDATE_KERNEL_ADDR 0x4e7b8

typedef void (*RenderKernelFn)(WorkItem*, void*, void*, int, int);

// init.cpp's temperature-sensor code path resolves a couple of
// library globals via resolve_ptr(), which needs this - unused by anything
// we actually exercise here (no fb/threads/temperature), but must be
// defined to satisfy the link.
static uintptr_t g_runtime_offset = 0;
uintptr_t
swtcon_runtime_offset() {
  return g_runtime_offset;
}

static uintptr_t
load_library() {
  void* handle =
    dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
  if (!handle) {
    handle = dlopen("./libqsgepaper.so", RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
      fprintf(stderr, "Failed to load libqsgepaper.so: %s\n", dlerror());
      exit(1);
    }
  }
  void* instance_func = dlsym(handle, "_ZN13EPFramebuffer8instanceEv");
  if (!instance_func) {
    fprintf(stderr, "Failed to find known export symbol: %s\n", dlerror());
    exit(1);
  }
  return (uintptr_t)instance_func - INSTANCE_ADDR;
}

// Effective dispatch case, mirroring lib_render_update_kernel's pixelMode==5
// ("auto") indirection through g_anPixelModeDispatchTable (confirmed by
// reading the table's bytes directly out of the loaded library at
// Ghidra address 0x596b8: {6,9,9,9,9,6,8} for mode 1..7).
static int
kernel_case(int pixel_mode, int mode) {
  if (pixel_mode == 5) {
    static const int kTable[7] = { 6, 9, 9, 9, 9, 6, 8 };
    unsigned idx = (unsigned)(mode - 1);
    if (idx > 6)
      return -1; // out-of-range mode falls through to the default formula
    return kTable[idx];
  }
  return pixel_mode;
}

// Per-pixel formula candidates, transcribed from the ARM disassembly at
// 0x4e7b8 (bitfield extraction, gamma-table add, and the div-by-125-then-
// scale sequences were read directly off the umull/lsr reciprocal-division
// idiom, not just the decompiler's pseudocode).
static uint8_t
kernel_formula(int case_, uint16_t src, bool back_active, uint8_t gamma) {
  unsigned lo5 = src & 0x1f;
  unsigned mid6 = (src >> 5) & 0x3f;
  unsigned hi5 = src >> 11;
  switch (case_) {
    case 6:
    case 8:
      return (uint8_t)(((lo5 + mid6 + hi5 + gamma) / 125) * 30);
    case 7:
      if (!back_active)
        return 0x20;
      return (uint8_t)(((lo5 + mid6 + hi5 + gamma) / 125) * 30);
    case 9:
      return (uint8_t)((((lo5 + mid6 + hi5) * 15 + gamma) / 125) << 1);
    case 0xd:
      return 0x1e;
    default:
      return (uint8_t)(((lo5 + mid6 + hi5) >> 3) << 1);
  }
}

struct TestConfig {
  const char* name;
  int pixel_mode;
  int mode; // only meaningful when pixel_mode == 5
  uint8_t back_fill;
};

static const TestConfig kConfigs[] = {
  { "case6",                 6, 0, 0xff },
  { "case7-active",          7, 0, 0xff },
  { "case7-gated",           7, 0, 0x00 },
  { "case8",                 8, 0, 0xff },
  { "case9",                 9, 0, 0xff },
  { "case0xd",                0xd, 0, 0xff },
  { "default-0xa",            0xa, 0, 0xff },
  { "default-0xb",            0xb, 0, 0xff },
  { "default-0xc",            0xc, 0, 0xff },
  { "auto-mode1(->case6)",    5, 1, 0xff },
  { "auto-mode2(->case9)",    5, 2, 0xff },
  { "auto-mode3(->case9)",    5, 3, 0xff },
  { "auto-mode4(->case9)",    5, 4, 0xff },
  { "auto-mode5(->case9)",    5, 5, 0xff },
  { "auto-mode6(->case6)",    5, 6, 0xff },
  { "auto-mode7(->case8)",    5, 7, 0xff },
};

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

int
main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = segv_handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, nullptr);

  g_runtime_offset = load_library();
  printf("Loaded library, runtime_offset=0x%lx\n", (unsigned long)g_runtime_offset);
  auto lib_render_update_kernel =
    (RenderKernelFn)(g_runtime_offset + RENDER_UPDATE_KERNEL_ADDR);

  if (init_statebuffer() != 0) {
    fprintf(stderr, "init_statebuffer failed\n");
    return 1;
  }
  // lib_render_update_kernel reads the LIBRARY's own g_pGammaTable global (not
  // a parameter) - init_statebuffer already points statebuffer_globals()'s
  // pGammaTable at our native table, exactly like swtcon_init does for the
  // real init path.
  printf("init_statebuffer done\n");

  const int W = 1404, H = 1872;
  uint16_t* dataBuffer = (uint16_t*)update_queue_globals()->dataBuffer;
  uint8_t* backBuffer = (uint8_t*)update_queue_globals()->backBuffer;
  uint8_t* gammaTable = (uint8_t*)statebuffer_globals()->pGammaTable;

  const int RECT = 128;
  uint8_t* output = (uint8_t*)malloc(RECT * RECT);

  // Sweep stride across the 16-bit src space; pass 1 on the command line for
  // an exhaustive (slow) run. Default is a coarse-but-covering pass.
  int step = argc > 1 ? atoi(argv[1]) : 1009;
  if (step <= 0)
    step = 1;
  printf("Sweep step=%d (pass 1 for exhaustive 0..0xffff)\n", step);

  int total_fail = 0;

  for (const auto& cfg : kConfigs) {
    int case_ = kernel_case(cfg.pixel_mode, cfg.mode);
    memset(backBuffer, cfg.back_fill, (size_t)W * H);
    bool gamma_dependent = (case_ == 6 || case_ == 7 || case_ == 8 || case_ == 9);
    bool config_failed = false;

    for (uint32_t v = 0; v <= 0xffff && !config_failed; v += step) {
      uint16_t src = (uint16_t)v;

      for (int i = 0; i < W * H; i++)
        dataBuffer[i] = src;

      WorkItem item{};
      RegionRows rr;
      rr.dataPtr = output;
      rr.y0 = 0;
      rr.x0 = 0;
      rr.y1 = RECT - 1;
      rr.x1 = RECT - 1;
      rr.stride = RECT;
      rr.size = RECT * RECT;
      memset(output, 0xcc, RECT * RECT); // poison, to catch unwritten bytes

      item.regionRows = non_owning_sp(&rr);
      item.pixelDataPtr = (int32_t)(intptr_t)rr.dataPtr;
      item.rectY0 = 0;
      item.rectX0 = 0;
      item.rectY1 = RECT - 1;
      item.rectX1 = RECT - 1;
      item.mode = (int16_t)cfg.mode;
      item.pixelMode = cfg.pixel_mode;

      lib_render_update_kernel(&item, dataBuffer, backBuffer, 0, 1);

      if (case_ == 0xd) {
        for (int i = 0; i < RECT * RECT; i++) {
          if (output[i] != 0x1e) {
            printf("FAIL [%s] v=0x%04x: byte[%d]=0x%02x expected 0x1e\n", cfg.name,
                   v, i, output[i]);
            config_failed = true;
            break;
          }
        }
      } else if (!gamma_dependent) {
        uint8_t expected = kernel_formula(case_, src, cfg.back_fill != 0, 0);
        for (int i = 0; i < RECT * RECT; i++) {
          if (output[i] != expected) {
            printf("FAIL [%s] v=0x%04x: byte[%d]=0x%02x expected 0x%02x\n", cfg.name,
                   v, i, output[i], expected);
            config_failed = true;
            break;
          }
        }
      } else {
        std::vector<uint8_t> expected;
        expected.reserve(RECT * RECT);
        for (int a = 0; a < 128; a++)
          for (int b = 0; b < 128; b++)
            expected.push_back(
              kernel_formula(case_, src, cfg.back_fill != 0, gammaTable[a + b * 0x88]));
        std::vector<uint8_t> actual(output, output + RECT * RECT);
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        if (expected != actual) {
          printf("FAIL [%s] v=0x%04x: multiset mismatch\n", cfg.name, v);
          for (size_t i = 0; i < expected.size(); i++) {
            if (expected[i] != actual[i]) {
              printf("  first diff at sorted idx %zu: expected=0x%02x actual=0x%02x\n", i,
                     expected[i], actual[i]);
              break;
            }
          }
          config_failed = true;
        }
      }
    }

    if (config_failed)
      total_fail++;
    printf("%-24s %s\n", cfg.name, config_failed ? "FAIL" : "PASS");
  }

  free(output);
  if (total_fail)
    printf("\n=== %d configuration(s) FAILED ===\n", total_fail);
  else
    printf("\n=== all configurations PASSED ===\n");
  return total_fail ? 1 : 0;
}
