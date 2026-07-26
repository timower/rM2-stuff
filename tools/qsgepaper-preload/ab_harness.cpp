// A/B regression harness for the swtcon display pipeline.
//
// Runs a fixed matrix of update sequences through the real pipeline and, when
// SWTCON_AB_CAPTURE=<path> is set, records the transient waveform frames + sp3
// state + settled buffers (see ab_capture.h). Run it twice on identical input -
// once native, once with SWTCON_LIBDISPATCH=1 to route dispatch through the
// still-library implementation - then diff the two capture files to prove the
// native port drives the panel byte-for-byte like the library. This catches the
// class of bug (e.g. the WorkItem.stateDataPtr +0x44 rebasing bug) that is
// invisible in every settled buffer and only shows in the transient drive data.
//
// Usage:
//   swtcon-ab-test                       run the matrix, capture to
//                                         $SWTCON_AB_CAPTURE (no-op if unset)
//   swtcon-ab-test --compare A B          diff two capture files; exit 1 if they
//                                         differ, 0 if identical
//
// Typical loop (on the emulator, where the real libqsgepaper.so is present):
//   SWTCON_AB_CAPTURE=/tmp/native.txt                     ./swtcon-ab-test
//   SWTCON_AB_CAPTURE=/tmp/lib.txt SWTCON_LIBDISPATCH=1    ./swtcon-ab-test
//   ./swtcon-ab-test --compare /tmp/native.txt /tmp/lib.txt
//
// The cases deliberately exercise narrowed/split items (overlap splits, non-8-
// aligned rects, cross-batch overdraw) because that is where the sp3 buffer's
// rebase offset is nonzero - the exact condition the +0x44 bug got wrong.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

#include "ab_capture.h"
#include "swtcon.h"

namespace {

constexpr int SCREEN_W = 1404;
constexpr int SCREEN_H = 1872;

enum Mode { HQ = 2, MEDIUM = 3, FAST = 4 };
enum Flags { Sync = 1 << 0, FullRefresh = 1 << 1 };

uint16_t* g_image = nullptr;

update_data
rect_req(int y0, int x0, int y1, int x1, int mode = HQ, int pixel_mode = 9) {
  return update_data{ y0, x0, y1, x1, 0, mode, 0, pixel_mode };
}

void
fill_rect(int y0, int x0, int y1, int x1, uint16_t value) {
  for (int y = y0; y < y1 && y < SCREEN_H; y++)
    for (int x = x0; x < x1 && x < SCREEN_W; x++)
      g_image[y * SCREEN_W + x] = value;
}

void
do_batch(std::initializer_list<update_data> reqs) {
  swtcon_lock();
  for (update_data req : reqs)
    swtcon_update(&req);
  swtcon_unlock_post();
  swtcon_wait();
}

// --- The input matrix. Each case ends by capturing the settled state so a
//     divergence is pinned to a named step. ---

void
run_matrix() {
  // 1. Two disjoint regions in one batch (no split).
  fill_rect(0, 0, SCREEN_H, SCREEN_W, 0xFFFF);
  fill_rect(0, 0, 800, 600, 0x0);
  fill_rect(1100, 900, SCREEN_H, SCREEN_W, 0x0);
  do_batch({ rect_req(0, 0, 800, 600), rect_req(1100, 900, SCREEN_H, SCREEN_W) });
  ab_capture_settled("1_nonoverlap");

  // 2. Hole punched in the middle of a pending region -> all four split pieces.
  fill_rect(0, 0, SCREEN_H, SCREEN_W, 0xFFFF);
  fill_rect(200, 200, 1600, 1200, 0x0);
  fill_rect(600, 500, 1200, 800, 0xFFFF);
  do_batch({ rect_req(200, 200, 1600, 1200), rect_req(600, 500, 1200, 800) });
  ab_capture_settled("2_hole_punch");

  // 3. Single-edge overlap -> exactly one split piece.
  fill_rect(0, 0, SCREEN_H, SCREEN_W, 0xFFFF);
  fill_rect(300, 300, 1400, 900, 0x0);
  fill_rect(300, 700, 1400, 1200, 0x8410);
  do_batch({ rect_req(300, 300, 1400, 900), rect_req(300, 700, 1400, 1200) });
  ab_capture_settled("3_single_edge");

  // 4. Second region fully contains the first -> first fully removed.
  fill_rect(0, 0, SCREEN_H, SCREEN_W, 0xFFFF);
  fill_rect(700, 600, 1100, 900, 0x0);
  fill_rect(100, 100, 1700, 1300, 0x8410);
  do_batch({ rect_req(700, 600, 1100, 900), rect_req(100, 100, 1700, 1300) });
  ab_capture_settled("4_full_contain");

  // 5. Corner overlap -> two split pieces on non-trivial edges.
  fill_rect(0, 0, SCREEN_H, SCREEN_W, 0xFFFF);
  fill_rect(400, 400, 1000, 1000, 0x0);
  fill_rect(800, 800, 1400, 1400, 0x8410);
  do_batch({ rect_req(400, 400, 1000, 1000), rect_req(800, 800, 1400, 1400) });
  ab_capture_settled("5_corner");

  // 6. Deliberately non-8-row-aligned origins and odd widths, so the sp3 seed
  //    rect (clamped outward to the 8-row NEON group) sits below the item's
  //    real origin and the +0x44 rebase offset is nonzero on every piece.
  fill_rect(0, 0, SCREEN_H, SCREEN_W, 0xFFFF);
  fill_rect(203, 101, 1607, 1203, 0x0);
  fill_rect(605, 503, 1203, 805, 0xFFFF);
  do_batch({ rect_req(203, 101, 1607, 1203), rect_req(605, 503, 1203, 805) });
  ab_capture_settled("6_unaligned_split");

  // 7. Cross-batch overdraw (the original artifact scenario): draw a region and
  //    let it settle, then in a SEPARATE batch draw an overlapping region so the
  //    second update's transitions are computed against the first's settled
  //    state.
  fill_rect(0, 0, SCREEN_H, SCREEN_W, 0xFFFF);
  fill_rect(250, 250, 1550, 1150, 0x0);
  do_batch({ rect_req(250, 250, 1550, 1150) });
  ab_capture_settled("7a_overdraw_base");
  fill_rect(550, 450, 1250, 850, 0x8410);
  do_batch({ rect_req(550, 450, 1250, 850) });
  ab_capture_settled("7b_overdraw_over");
}

} // namespace

int
main(int argc, char** argv) {
  if (argc >= 4 && strcmp(argv[1], "--compare") == 0)
    return ab_capture_compare(argv[2], argv[3]);
  if (argc >= 2 && strcmp(argv[1], "--compare") == 0) {
    fprintf(stderr, "usage: %s --compare <fileA> <fileB>\n", argv[0]);
    return 2;
  }

  g_image = swtcon_init();
  if (!g_image) {
    fprintf(stderr, "swtcon_init failed\n");
    return 1;
  }
  if (!ab_capture_enabled())
    fprintf(stderr, "note: SWTCON_AB_CAPTURE not set - running matrix without capture\n");

  run_matrix();

  swtcon_shutdown(0);
  ab_capture_flush();
  return 0;
}
