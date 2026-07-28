// Empirically maps lib_render_update_kernel's (0x4e7b8) input-pixel -> output-
// byte addressing/rotation, which render_kernel_verify.cpp deliberately
// sidesteps (see its header comment). Rather than hand-decoding the ARM
// pointer arithmetic (chunked, 8-wide-NEON, 180-deg-rotated), this uses a
// reversed binary search per output byte: since each output byte is a pure
// function of exactly one dataBuffer pixel (per the confirmed per-pixel
// formulas), setting a contiguous half of the flattened dataBuffer to a
// distinct "marker" value and checking whether a chosen output byte flips to
// the marker's formula result tells us which half contains the influencing
// pixel - repeat ~log2(W*H) times to pin it down exactly, with zero
// assumptions about locality or rotation direction.
// See AGENTS.md Phase 6 / swtcon_architecture.md §5.1.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

#include "init.h"
#include "update.h"

#define INSTANCE_ADDR 0x35de0
#define RENDER_UPDATE_KERNEL_ADDR 0x4e7b8

extern void* g_pImageBufferNative;
extern void* g_pScreenBufferNative;
extern void* g_pGammaTableNative;

typedef void (*RenderKernelFn)(WorkItem*, void*, void*, int, int);

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

int
main(int argc, char** argv) {
  g_runtime_offset = load_library();
  auto lib_render_update_kernel =
    (RenderKernelFn)(g_runtime_offset + RENDER_UPDATE_KERNEL_ADDR);

  if (init_statebuffer() != 0) {
    fprintf(stderr, "init_statebuffer failed\n");
    return 1;
  }
  statebuffer_globals()->pGammaTable = g_pGammaTableNative;

  const int W = 1404, H = 1872;
  uint16_t* dataBuffer = (uint16_t*)g_pImageBufferNative;
  uint8_t* backBuffer = (uint8_t*)g_pScreenBufferNative;
  memset(backBuffer, 0xff, (size_t)W * H);

  const int RECT = 128;
  uint8_t* output = (uint8_t*)malloc(RECT * RECT);

  int rectY0 = argc > 1 ? atoi(argv[1]) : 0;
  int rectX0 = argc > 2 ? atoi(argv[2]) : 0;

  const uint16_t B0 = 0x0000; // formula(0xa, B0) = 0
  const uint16_t B1 = 0xffff; // formula(0xa, B1) = 0x1e - distinct marker
  const int64_t N = (int64_t)W * H;

  auto fill_range = [&](int64_t lo, int64_t hi, uint16_t v) {
    for (int64_t i = lo; i < hi; i++)
      dataBuffer[i] = v;
  };
  fill_range(0, N, B0);

  WorkItem item{};
  RegionRows rr;
  rr.dataPtr = output;
  rr.y0 = rectY0;
  rr.x0 = rectX0;
  rr.y1 = rectY0 + RECT - 1;
  rr.x1 = rectX0 + RECT - 1;
  rr.stride = RECT;
  rr.size = RECT * RECT;
  item.regionRows = non_owning_sp(&rr);
  item.pixelDataPtr = (int32_t)(intptr_t)rr.dataPtr;
  item.rectY0 = rectY0;
  item.rectX0 = rectX0;
  item.rectY1 = rectY0 + RECT - 1;
  item.rectX1 = rectX0 + RECT - 1;
  item.mode = 0;
  item.pixelMode = 0xa; // default (gamma-free) case

  printf("rect=[y0=%d x0=%d y1=%d x1=%d]\n", rectY0, rectX0, rectY0 + RECT - 1, rectX0 + RECT - 1);

  // Sample a handful of representative output positions: the four corners
  // and center of the 128x128 RegionRows output grid (column-major:
  // offset = stride*col + row).
  struct Sample { int col, row; };
  Sample samples[] = {
    { 0, 0 }, { 0, RECT - 1 }, { RECT - 1, 0 }, { RECT - 1, RECT - 1 }, { RECT / 2, RECT / 2 },
  };

  auto run_kernel_check = [&](int target_offset) -> bool {
    memset(output, 0xcc, RECT * RECT);
    lib_render_update_kernel(&item, dataBuffer, backBuffer, 0, 1);
    return output[target_offset] == 0x1e;
  };

  for (auto& s : samples) {
    int target_offset = s.col * RECT + s.row;

    // Sanity: with the whole buffer at B0, target must read as formula(B0)=0,
    // not the marker - otherwise something else is wrong (aliasing, wrong
    // rect, etc.) and the binary search below would be meaningless.
    fill_range(0, N, B0);
    if (run_kernel_check(target_offset)) {
      printf("col=%3d row=%3d -> BASELINE ALREADY MARKED, aborting this sample\n", s.col, s.row);
      continue;
    }

    int64_t lo = 0, hi = N;
    while (hi - lo > 1) {
      int64_t mid = lo + (hi - lo) / 2;
      fill_range(0, N, B0);
      fill_range(lo, mid, B1);
      bool marked = run_kernel_check(target_offset);
      if (marked)
        hi = mid;
      else
        lo = mid;
    }
    fill_range(0, N, B0);

    int64_t abs_y = lo / W, abs_x = lo % W;
    printf("col=%3d row=%3d -> influencing dataBuffer flat_idx=%lld (abs_y=%lld abs_x=%lld) "
           "rel_y=%lld rel_x=%lld\n",
           s.col, s.row, (long long)lo, (long long)abs_y, (long long)abs_x,
           (long long)(abs_y - rectY0), (long long)(abs_x - rectX0));
  }

  // Phase 2: gamma-table index. We own the native gamma buffer (already
  // pointed at from the library's own g_pGammaTable global above), so we can
  // mark individual gamma cells directly instead of inverting a lossy
  // (sum+gamma)/125 formula. Hold src=0 uniform (sum=0) under case 8
  // (unconditional, gamma-dependent): baseline output is (0+0)/125*30=0;
  // marking one gamma byte to 255 gives (0+255)/125*30=2*30=0x3c, a value
  // unreachable under any normal (0-124) gamma content with sum=0.
  printf("\n--- gamma index ---\n");
  fill_range(0, N, 0x0000);
  uint8_t* gammaTable = (uint8_t*)g_pGammaTableNative;
  const int64_t GN = 0x4400;
  uint8_t gamma_backup[0x4400];
  memcpy(gamma_backup, gammaTable, GN);
  item.pixelMode = 8;

  auto fill_gamma_range = [&](int64_t lo, int64_t hi, uint8_t v) {
    for (int64_t i = lo; i < hi; i++)
      gammaTable[i] = v;
  };

  for (auto& s : samples) {
    int target_offset = s.col * RECT + s.row;

    memcpy(gammaTable, gamma_backup, GN);
    fill_gamma_range(0, GN, 0x00);
    if (run_kernel_check(target_offset) || output[target_offset] != 0x00) {
      printf("col=%3d row=%3d -> BASELINE UNEXPECTED (0x%02x), aborting this sample\n", s.col,
             s.row, output[target_offset]);
      continue;
    }

    int64_t lo = 0, hi = GN;
    while (hi - lo > 1) {
      int64_t mid = lo + (hi - lo) / 2;
      fill_gamma_range(0, GN, 0x00);
      fill_gamma_range(lo, mid, 0xff);
      memset(output, 0xcc, RECT * RECT);
      lib_render_update_kernel(&item, dataBuffer, backBuffer, 0, 1);
      bool marked = output[target_offset] == 0x3c;
      if (marked)
        hi = mid;
      else
        lo = mid;
    }
    fill_gamma_range(0, GN, 0x00);

    int64_t a = lo % 0x88, b = lo / 0x88;
    printf("col=%3d row=%3d -> gamma flat_idx=%lld (a=%lld b=%lld)\n", s.col, s.row, (long long)lo,
           (long long)a, (long long)b);
  }
  memcpy(gammaTable, gamma_backup, GN);

  // Phase 3: does backBuffer (case 7's gate) share the same rotated
  // (src_y,src_x) addressing as dataBuffer? Test at rect-local (col=0,row=0):
  // per the confirmed formula, src_y=(H-1)-(rectY0+0), src_x=(W-1)-(rectX0+0).
  printf("\n--- backBuffer gate (case 7), spot check at col=0 row=0 ---\n");
  int64_t src_y0 = (H - 1) - rectY0;
  int64_t src_x0 = (W - 1) - rectX0;
  int64_t flat0 = src_y0 * W + src_x0;
  fill_range(0, N, 0xffff); // src=0xffff, active formula = (125+gamma)/125*30 != 0x20
  memset(backBuffer, 0x00, (size_t)N);
  backBuffer[flat0] = 0xff;
  item.pixelMode = 7;
  memset(output, 0xcc, RECT * RECT);
  lib_render_update_kernel(&item, dataBuffer, backBuffer, 0, 1);
  printf("predicted backBuffer flat_idx=%lld (src_y=%lld src_x=%lld): output[0]=0x%02x "
         "(0x20=gated/inactive, anything else=confirmed active)\n",
         (long long)flat0, (long long)src_y0, (long long)src_x0, output[0]);

  free(output);
  return 0;
}
