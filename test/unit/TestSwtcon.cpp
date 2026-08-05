#include <catch2/catch_test_macros.hpp>

#include "TempFiles.h"

#include "init.h"
#include "update.h"
#include "SwtconFixture.h"

#include <chrono>
#include <fstream>
#include <memory>

using swtcon_test::run_with_timeout;
using swtcon_test::SwtconFixture;

namespace {
WorkItem
make_item(int y0, int x0, int y1, int x1) {
  WorkItem item{};
  item.rectY0 = y0;
  item.rectX0 = x0;
  item.rectY1 = y1;
  item.rectX1 = x1;
  return item;
}
} // namespace

// --- init.cpp: find_waveform_path's portable sub-pieces (it itself hardcodes
// real device paths, so isn't directly host-testable). ---

TEST_CASE("has_wbf_suffix", "[swtcon]") {
  CHECK(has_wbf_suffix("foo.wbf"));
  CHECK(has_wbf_suffix("foo.WBF")); // case-insensitive
  CHECK(has_wbf_suffix("320_R333_ED103TC2U2.WbF"));
  CHECK_FALSE(has_wbf_suffix("foo.txt"));
  CHECK_FALSE(has_wbf_suffix("wbf")); // too short to hold a suffix at all
}

TEST_CASE("list_wbf_files", "[swtcon]") {
  TemporaryDirectory dir;
  std::ofstream(dir.dir / "a.wbf") << "x";
  std::ofstream(dir.dir / "b.WBF") << "x";
  std::ofstream(dir.dir / "c.txt") << "x";

  std::vector<std::string> found;
  list_wbf_files(dir.dir.string() + "/", &found);

  CHECK(found.size() == 2);
  for (auto& f : found) {
    CHECK(has_wbf_suffix(f.c_str()));
  }
}

TEST_CASE("list_wbf_files: missing directory is silently skipped", "[swtcon]") {
  std::vector<std::string> found;
  list_wbf_files("/no/such/directory/", &found);
  CHECK(found.empty());
}

// decode_fpl_lot_pair('A','D') == 333 is empirically confirmed against a
// real device's barcode/waveform pairing - see init.cpp's own comment.
TEST_CASE("decode_fpl_lot_pair", "[swtcon]") {
  CHECK(decode_fpl_lot_pair('A', 'D') == 333);
  CHECK(decode_fpl_lot_pair('0', '0') == -1); // explicitly rejected special case
  CHECK(decode_fpl_lot_pair('0', '1') == 1);
  CHECK(decode_fpl_lot_pair('9', '9') == 99);
  CHECK(decode_fpl_lot_pair('!', '0') == -1); // not a digit/valid letter at all
}

// This 25-byte barcode's decode is confirmed against a real device's
// actual waveform filename (see read_factory_barcode's own comment).
TEST_CASE("decode_barcode", "[swtcon]") {
  char tft_vid[3];
  int fpl_lot = -1;
  REQUIRE(decode_barcode("EUFZBRAD1V9V00A6TAT -1.39", tft_vid, &fpl_lot));
  CHECK(std::string(tft_vid) == "U2");
  CHECK(fpl_lot == 333);

  SECTION("wrong length is rejected") {
    CHECK_FALSE(decode_barcode("tooshort", tft_vid, &fpl_lot));
  }

  SECTION("unknown TFT_VID prefix falls back to wildcard, not failure") {
    // byte[5] must still be 'R' with a valid FPL_LOT pair for this to
    // succeed - only the TFT_VID lookup itself is allowed to miss.
    REQUIRE(decode_barcode("XXXZBRAD1V9V00A6TAT -1.39", tft_vid, &fpl_lot));
    CHECK(std::string(tft_vid) == "00");
    CHECK(fpl_lot == 333);
  }

  SECTION("missing FPL_LOT marker fails the whole decode") {
    CHECK_FALSE(decode_barcode("EUFZBXAD1V9V00A6TAT -1.39", tft_vid, &fpl_lot));
  }
}

// --- update.cpp: clamp_update_rect (180-degree reflection + 8-row align) ---

TEST_CASE("clamp_update_rect: full screen", "[swtcon]") {
  // Matches swtcon_update's own XYRect{data->x0,data->y0,data->x1,data->y1}
  // construction for a full-screen request (see main.cpp's HQ test case,
  // which sets update_data's y1/x1 to SCREEN_HEIGHT/SCREEN_WIDTH).
  Rect out = clamp_update_rect(XYRect{ 0, 0, kScreenWidth, kScreenHeight });
  CHECK(out.y0 == 0);
  CHECK(out.x0 == 0);
  CHECK(out.y1 == kScreenHeight - 1);
  CHECK(out.x1 == kScreenWidth - 1);
}

TEST_CASE("clamp_update_rect: y-axis rounds out to 8-row groups", "[swtcon]") {
  Rect out = clamp_update_rect(XYRect{ 0, 0, 1, 1 });
  CHECK(out.y0 == 1864); // (1871-1) rounded down to a multiple of 8
  CHECK(out.x0 == 1402);
  CHECK(out.y1 == 1871); // always ends in a "| 7" (xxx7) row
  CHECK(out.x1 == 1403);
}

// --- update.cpp: render_kernel_case (pixelMode==5 "auto" indirection) ---

TEST_CASE("render_kernel_case", "[swtcon]") {
  CHECK(render_kernel_case(9, 0) == 9);  // pixel_mode != 5: passed through
  CHECK(render_kernel_case(5, 1) == 6);
  CHECK(render_kernel_case(5, 2) == 9);
  CHECK(render_kernel_case(5, 5) == 9);
  CHECK(render_kernel_case(5, 6) == 6);
  CHECK(render_kernel_case(5, 7) == 8);
  CHECK(render_kernel_case(5, 8) == -1); // out of range mode -> default formula
}

// --- update.cpp: render_kernel_formula (per-pixel gamma/case formulas) ---

TEST_CASE("render_kernel_formula", "[swtcon]") {
  // src packs lo5 (bits 0-4) / mid6 (bits 5-10) / hi5 (bits 11-15).
  auto make_src = [](int lo5, int mid6, int hi5) -> uint16_t {
    return (uint16_t)(lo5 | (mid6 << 5) | (hi5 << 11));
  };
  uint16_t src = make_src(5, 10, 3); // lo5+mid6+hi5 == 18

  CHECK(render_kernel_formula(6, src, true, 150) == 30);  // (18+150)/125*30
  CHECK(render_kernel_formula(8, src, true, 150) == 30);  // case 8 == case 6

  SECTION("case 7 gates on backBuffer activity") {
    CHECK(render_kernel_formula(7, src, false, 150) == kGatedPixelSentinel);
    CHECK(render_kernel_formula(7, src, true, 150) == 30); // same formula as 6/8
  }

  CHECK(render_kernel_formula(9, src, true, 150) == 6);    // ((18*15+150)/125)<<1
  CHECK(render_kernel_formula(0xd, src, true, 150) == 0x1e); // fixed constant
  CHECK(render_kernel_formula(0, src, true, 150) == 4);    // default: (18>>3)<<1
}

// --- update.cpp: subtract_update_region (overlap clipping / splitting) ---

TEST_CASE("subtract_update_region: full containment removes the old item outright",
          "[swtcon]") {
  std::list<WorkItem> list;
  list.push_back(make_item(700, 600, 1100, 900));

  WorkItem punch = make_item(100, 100, 1700, 1300); // fully contains the above
  subtract_update_region(list, &punch);

  CHECK(list.empty());
}

TEST_CASE("subtract_update_region: no overlap leaves the old item untouched",
          "[swtcon]") {
  std::list<WorkItem> list;
  list.push_back(make_item(0, 0, 100, 100));

  WorkItem elsewhere = make_item(500, 500, 600, 600);
  subtract_update_region(list, &elsewhere);

  REQUIRE(list.size() == 1);
  CHECK(list.front().rectY0 == 0);
  CHECK(list.front().rectX1 == 100);
}

TEST_CASE("subtract_update_region: hole punched in the middle yields four strips",
          "[swtcon]") {
  std::list<WorkItem> list;
  list.push_back(make_item(200, 200, 1600, 1200));

  WorkItem punch = make_item(600, 500, 1200, 800); // strictly inside on every edge
  subtract_update_region(list, &punch);

  REQUIRE(list.size() == 4);

  // Left strip: old's own y-range, x from old's x0 up to the cut's x0-1.
  auto& left = list.front();
  CHECK(left.rectY0 == 200);
  CHECK(left.rectX0 == 200);
  CHECK(left.rectY1 == 1600);
  CHECK(left.rectX1 == 499);
}

TEST_CASE("subtract_update_region: single-edge overlap yields exactly one piece",
          "[swtcon]") {
  std::list<WorkItem> list;
  list.push_back(make_item(300, 300, 1400, 900));

  // Same y-range, x-range only partially overlaps on the right edge.
  WorkItem overlap = make_item(300, 700, 1400, 1200);
  subtract_update_region(list, &overlap);

  REQUIRE(list.size() == 1);
  CHECK(list.front().rectX0 == 300);
  CHECK(list.front().rectX1 == 699); // clipped to just before the overlap
}

// --- swtcon_init/shutdown against a fake fd + a real waveform (extracted
// from RemEmu, /usr/share/remarkable/*.wbf - see test.wbf's own comment in
// SwtconFixture.h) ---

TEST_CASE("SwtconFixture: init produces a working image buffer", "[swtcon]") {
  SwtconFixture fx;
  REQUIRE(fx.image != nullptr);

  auto* queue = update_queue_globals();
  REQUIRE(queue->waveform.size() == 8); // INIT/DU/GC16/GL16/.../A2/DU4
  for (auto* mode : queue->waveform) {
    REQUIRE(mode->luts.size() == 14); // 14 temperature buckets
    for (auto& lut : mode->luts) {
      CHECK(lut->mode_width == 32);
      CHECK(lut->bit_depth == 2);
      CHECK(lut->size_kb > 0);
      CHECK(lut->data != nullptr);
    }
  }
}

// --- swtcon_lock/update/unlock_post driven synchronously, no worker/display
// thread (SwtconFixture(startThreads=false)) - so swtcon_wait() would hang
// forever (nothing ever drains listIncomingUpdates) and isn't called here;
// these inspect queue state directly instead. Mirrors qsgepaper-test's own
// region-overlap scenarios (main.cpp), deterministically. ---

namespace {
update_data
rect_req(int y0, int x0, int y1, int x1, int mode, int pixel_mode, int flags = 0) {
  return update_data{ y0, x0, y1, x1, flags, mode, 0, pixel_mode };
}
} // namespace

TEST_CASE("swtcon_update: single request queues as one item", "[swtcon]") {
  SwtconFixture fx;
  swtcon_lock();
  update_data req = rect_req(0, 0, 800, 600, 2, 9);
  swtcon_update(&req);
  swtcon_unlock_post();

  auto* queue = update_queue_globals();
  REQUIRE(queue->listIncomingUpdates.size() == 1);
  CHECK(queue->listIncomingUpdates.front().subList.size() == 1);
}

TEST_CASE("swtcon_update: two non-overlapping regions batch together", "[swtcon]") {
  SwtconFixture fx;
  swtcon_lock();
  update_data r1 = rect_req(0, 0, 800, 600, 2, 9);
  swtcon_update(&r1);
  update_data r2 = rect_req(1100, 900, 1872, 1404, 2, 9);
  swtcon_update(&r2);
  swtcon_unlock_post();

  auto* queue = update_queue_globals();
  REQUIRE(queue->listIncomingUpdates.size() == 1);
  CHECK(queue->listIncomingUpdates.front().subList.size() == 2);
}

TEST_CASE("swtcon_update: hole punched in a pending region splits it into four",
          "[swtcon]") {
  SwtconFixture fx;
  swtcon_lock();
  update_data old_req = rect_req(200, 200, 1600, 1200, 2, 9);
  swtcon_update(&old_req);
  // Still under the same lock: subtract_update_region clips `old_req`
  // (already in accumList) against this one before it's appended itself.
  update_data punch = rect_req(600, 500, 1200, 800, 2, 9);
  swtcon_update(&punch);
  swtcon_unlock_post();

  auto* queue = update_queue_globals();
  REQUIRE(queue->listIncomingUpdates.size() == 1);
  CHECK(queue->listIncomingUpdates.front().subList.size() == 5); // 4 pieces + punch
}

TEST_CASE("swtcon_update: out-of-range mode is silently dropped", "[swtcon]") {
  SwtconFixture fx;
  swtcon_lock();
  update_data req = rect_req(0, 0, 100, 100, /*mode=*/99, 9);
  swtcon_update(&req);

  auto* queue = update_queue_globals();
  CHECK(queue->accumList.empty()); // update_lut_is_valid rejected it
  swtcon_unlock_post();
}

// --- Real worker/display threads (SwtconFixture(startThreads=true)) against
// the fake fd - a deliberately small set, since these are inherently
// timing-sensitive; run_with_timeout turns a hang into a failure instead of
// blocking the whole test binary. ---

TEST_CASE("swtcon threads: init/shutdown round-trip completes promptly", "[swtcon]") {
  auto fx = std::make_unique<SwtconFixture>(/*startThreads=*/true);
  bool finished = run_with_timeout([&] { fx.reset(); }, std::chrono::seconds(5));
  CHECK(finished);
}

TEST_CASE("swtcon threads: suspend/resume then shutdown completes promptly",
          "[swtcon]") {
  auto fx = std::make_unique<SwtconFixture>(/*startThreads=*/true);
  swtcon_suspend();
  swtcon_resume();
  bool finished = run_with_timeout([&] { fx.reset(); }, std::chrono::seconds(5));
  CHECK(finished);
}
