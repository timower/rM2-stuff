#include <catch2/catch_test_macros.hpp>

#include "TempFiles.h"

#include "display.h"
#include "init.h"
#include "update.h"
#include "MockFb.h"
#include "SwtconFixture.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <linux/fb.h>
#include <memory>
#include <vector>

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

// Builds a WorkItem with a real, owned RegionRows (matching
// dispatch_update_regions's own allocation idiom) filled uniformly with
// `fillValue` - for commit_item tests, which read the rendered pixel through
// item->pixelDataPtr/regionRows->stride.
WorkItem
make_rendered_item(int y0, int x0, int y1, int x1, uint8_t sync, uint8_t fillValue) {
  WorkItem item{};
  item.rectY0 = y0;
  item.rectX0 = x0;
  item.rectY1 = y1;
  item.rectX1 = x1;
  item.sync = sync;

  int rows = y1 - y0 + 1;
  int cols = x1 - x0 + 1;
  int stride = (rows + kRegionRowsStrideAlign - 1) & ~(kRegionRowsStrideAlign - 1);
  size_t size = (size_t)stride * cols;

  auto* rr = new RegionRows{};
  rr->y0 = y0;
  rr->x0 = x0;
  rr->y1 = y1;
  rr->x1 = x1;
  rr->stride = stride;
  rr->size = (int32_t)size;
  rr->dataPtr = new uint8_t[size];
  std::fill_n(rr->dataPtr, size, fillValue);

  item.regionRows = std::shared_ptr<RegionRows>(rr, [](RegionRows* p) {
    delete[] p->dataPtr;
    delete p;
  });
  item.pixelDataPtr = (uintptr_t)rr->dataPtr;
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

TEST_CASE("decode_fpl_lot_pair: covers every first/second-letter alphabet range", "[swtcon]") {
  CHECK(decode_fpl_lot_pair('J', '5') != -1); // first letter J-N -> digit second char
  CHECK(decode_fpl_lot_pair('J', 'B') == -1); // first letter J-N -> non-digit second char rejected
  CHECK(decode_fpl_lot_pair('Q', '3') != -1); // first letter Q-Z -> digit second char
  CHECK(decode_fpl_lot_pair('A', 'J') != -1); // first letter digit/A-H, second letter J-N
  CHECK(decode_fpl_lot_pair('A', 'Q') != -1); // first letter digit/A-H, second letter Q-Z
  CHECK(decode_fpl_lot_pair('A', 'I') == -1); // first letter digit/A-H, second char unmatched
}

TEST_CASE("create_pid_file: acquires an exclusive lock, rejects a second acquire, then releases",
          "[swtcon]") {
  REQUIRE(create_pid_file() == 0);
  // A second open+flock against the same path contends with the first
  // (still-held) fd's exclusive lock, even from this same process - flock is
  // per open-file-description, not per-process.
  CHECK(create_pid_file() == -1);
  unlock_pid_file();
  swtcon_state()->nPidFd = -1; // don't leave a stale closed fd for later tests' shutdown to touch
}

TEST_CASE("unlock_pid_file: reports (but survives) a flock failure on an already-invalid fd",
          "[swtcon]") {
  swtcon_state()->nPidFd = 123456; // not a fd this process actually owns
  REQUIRE_NOTHROW(unlock_pid_file());
  swtcon_state()->nPidFd = -1; // don't leave a bogus fd for a later test's swtcon_shutdown to touch
}

TEST_CASE("read_wbf_match_fields", "[swtcon]") {
  uint16_t fpl_lot;
  char tft_vid[3];

  SECTION("missing file fails") {
    CHECK_FALSE(read_wbf_match_fields("/no/such/file.wbf", &fpl_lot, tft_vid));
  }

  SECTION("too-small file fails") {
    TemporaryDirectory dir;
    auto path = dir.dir / "tiny.wbf";
    std::ofstream(path) << "short";
    CHECK_FALSE(read_wbf_match_fields(path.string(), &fpl_lot, tft_vid));
  }

  SECTION("real waveform file parses") {
    CHECK(read_wbf_match_fields(swtcon_test::test_wbf_path().string(), &fpl_lot, tft_vid));
  }
}

TEST_CASE("read_temperature_raw", "[swtcon]") {
  int value = 0;
  TemporaryDirectory dir;

  SECTION("missing file fails") {
    CHECK_FALSE(read_temperature_raw((dir.dir / "missing").string().c_str(), &value));
  }

  SECTION("empty file fails") {
    auto path = dir.dir / "empty";
    std::ofstream out(path);
    out.close();
    CHECK_FALSE(read_temperature_raw(path.string().c_str(), &value));
  }

  SECTION("a leading NUL byte fails, distinct from a genuinely empty read") {
    auto path = dir.dir / "nul";
    std::ofstream out(path, std::ios::binary);
    out.write("\0abc", 4);
    out.close();
    CHECK_FALSE(read_temperature_raw(path.string().c_str(), &value));
  }

  SECTION("non-numeric content fails") {
    auto path = dir.dir / "nan";
    std::ofstream(path) << "not-a-number";
    CHECK_FALSE(read_temperature_raw(path.string().c_str(), &value));
  }

  SECTION("out-of-range value fails") {
    auto path = dir.dir / "huge";
    std::ofstream(path) << "999999999999999999999999999999";
    CHECK_FALSE(read_temperature_raw(path.string().c_str(), &value));
  }

  SECTION("valid digits parse") {
    auto path = dir.dir / "valid";
    std::ofstream(path) << "23000";
    REQUIRE(read_temperature_raw(path.string().c_str(), &value));
    CHECK(value == 23000);
  }
}

TEST_CASE("load_waveform", "[swtcon]") {
  std::vector<ModeEntry*> waveform;

  SECTION("missing file fails") {
    CHECK_FALSE(load_waveform(&waveform, "/no/such/file.wbf"));
  }

  SECTION("too-small file fails") {
    TemporaryDirectory dir;
    auto path = dir.dir / "tiny.wbf";
    std::ofstream(path) << "short";
    CHECK_FALSE(load_waveform(&waveform, path.string().c_str()));
  }

  SECTION("header size field not matching the real file size fails") {
    TemporaryDirectory dir;
    auto path = dir.dir / "mismatch.wbf";
    std::vector<uint8_t> buf(0x40, 0);
    uint32_t wrong_hdr_sz = 0x1000; // doesn't match the real file size (0x40)
    std::memcpy(buf.data() + 4, &wrong_hdr_sz, sizeof(wrong_hdr_sz));
    std::ofstream out(path, std::ios::binary);
    out.write((const char*)buf.data(), (std::streamsize)buf.size());
    out.close();
    CHECK_FALSE(load_waveform(&waveform, path.string().c_str()));
  }
}

// write_lut_pattern (init.cpp's fill_lut_pattern, shared by init_lut/
// upload_lut_to_frame_slot's real waveform LUT and the worker thread's flash
// priming) - hand-verified against a few deterministic word offsets, chosen
// so the OR/AND base pattern, the pattern-parameter bits, and the
// end-to-end block-replication loop are all independently checked.
TEST_CASE("write_lut_pattern: dither-fill pattern matches the byte-verified formula",
          "[swtcon]") {
  std::vector<uint8_t> buf(kLutBlobSize);
  auto* words = (uint32_t*)buf.data();

  write_lut_pattern(buf.data(), 0);
  CHECK(words[0] == 0x430000u);      // base fill, outside every OR/AND range
  CHECK(words[0x28] == 0x450000u);   // inside both the OR [0x14,0x8e] and AND [0x28,0x66] ranges
  CHECK(words[0x326] == 0x510000u);  // third init_lut_sub block, param==0

  write_lut_pattern(buf.data(), 0xABCD);
  CHECK(words[0] == 0x430000u);        // pattern-independent region unaffected
  CHECK(words[0x326] == 0x51ABCDu);    // pattern's low 16 bits ORed in here
  // Every later 0x104-word block from buf+0x410 up to the end is a verbatim
  // copy of the buf+0x30c block - check the second block and the very last
  // one so both the replication loop's start and its end are covered.
  CHECK(words[0x42a] == words[0x326]);
  CHECK(words[0x594fc + 0x1a] == words[0x326]);
}

TEST_CASE("prime_display: already-unblanked panel skips the real pan/reblank sequence",
          "[swtcon]") {
  SwtconFixture fx;
  framebuffer_globals()->nIsFbBlanked = 0;
  REQUIRE_NOTHROW(prime_display());
  CHECK(framebuffer_globals()->nIsFbBlanked == 0); // early-return path leaves it untouched
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

  // Emission order is left, top, bottom, right (subtract_update_region's own
  // piece_count++ order) - cut = {y0:600,x0:500,y1:1200,x1:800}.
  auto it = list.begin();

  // Left strip: old's own y-range, x from old's x0 up to the cut's x0-1.
  auto& left = *it++;
  CHECK(left.rectY0 == 200);
  CHECK(left.rectX0 == 200);
  CHECK(left.rectY1 == 1600);
  CHECK(left.rectX1 == 499);

  // Top strip: cut's own x-range, y from old's y0 up to the cut's y0-1.
  auto& top = *it++;
  CHECK(top.rectY0 == 200);
  CHECK(top.rectX0 == 500);
  CHECK(top.rectY1 == 599);
  CHECK(top.rectX1 == 800);

  // Bottom strip: cut's own x-range, y from the cut's y1+1 down to old's y1.
  auto& bottom = *it++;
  CHECK(bottom.rectY0 == 1201);
  CHECK(bottom.rectX0 == 500);
  CHECK(bottom.rectY1 == 1600);
  CHECK(bottom.rectX1 == 800);

  // Right strip: old's own y-range, x from the cut's x1+1 up to old's x1.
  auto& right = *it++;
  CHECK(right.rectY0 == 200);
  CHECK(right.rectX0 == 801);
  CHECK(right.rectY1 == 1600);
  CHECK(right.rectX1 == 1200);
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

TEST_CASE("subtract_update_region: degenerate new rect is a no-op", "[swtcon]") {
  std::list<WorkItem> list;
  list.push_back(make_item(0, 0, 100, 100));

  WorkItem degenerate = make_item(50, 50, 10, 10); // y1<y0 and x1<x0
  subtract_update_region(list, &degenerate);

  REQUIRE(list.size() == 1);
  CHECK(list.front().rectY1 == 100);
}

TEST_CASE("list_insert_before / list_unhook: circular intrusive list primitives", "[swtcon]") {
  struct Node {
    void* next;
    void* prev;
  };
  Node sentinel{ &sentinel, &sentinel };
  Node a{}, b{};

  list_insert_before(&sentinel, &a); // append a at the tail
  CHECK(sentinel.next == &a);
  CHECK(sentinel.prev == &a);
  CHECK(a.next == &sentinel);
  CHECK(a.prev == &sentinel);

  list_insert_before(&sentinel, &b); // append b after a
  CHECK(sentinel.prev == &b);
  CHECK(a.next == &b);
  CHECK(b.prev == &a);
  CHECK(b.next == &sentinel);

  list_unhook(&a);
  CHECK(sentinel.next == &b);
  CHECK(b.prev == &sentinel);
}

TEST_CASE("BatchNodeClaimed: reads the claimed byte at +0x15", "[swtcon]") {
  BatchNode node{};
  std::memset(&node, 0, sizeof(node));
  CHECK_FALSE(BatchNodeClaimed(&node));
  *((uint8_t*)&node + 0x15) = 1;
  CHECK(BatchNodeClaimed(&node));
}

TEST_CASE("render_update_kernel: no-op when regionRows has no data buffer", "[swtcon]") {
  SwtconFixture fx; // provides statebuffer_globals()->pGammaTable
  WorkItem item{};
  item.rectY0 = 0;
  item.rectX0 = 0;
  item.rectY1 = 7;
  item.rectX1 = 7;
  item.pixelMode = 9;
  item.mode = 2;
  auto rr = std::make_shared<RegionRows>();
  rr->dataPtr = nullptr;
  item.regionRows = rr;
  uint16_t dataBuffer[1] = { 0 };
  uint8_t backBuffer[1] = { 0 };
  REQUIRE_NOTHROW(render_update_kernel(&item, dataBuffer, backBuffer));
}

TEST_CASE("update_lut_is_valid: rejects null data / non-positive fields", "[swtcon]") {
  auto lut = std::make_shared<LUTEntry>();

  lut->data = nullptr;
  CHECK_FALSE(update_lut_is_valid(lut));

  lut->data = malloc(4);
  lut->size_kb = 0;
  lut->bit_depth = 1;
  lut->mode_width = 1;
  CHECK_FALSE(update_lut_is_valid(lut)); // size_kb <= 0

  lut->size_kb = 1;
  lut->bit_depth = 0;
  CHECK_FALSE(update_lut_is_valid(lut)); // bit_depth <= 0

  lut->size_kb = 1;
  lut->bit_depth = 1;
  lut->mode_width = 0;
  CHECK_FALSE(update_lut_is_valid(lut)); // mode_width <= 0

  lut->mode_width = 1;
  CHECK(update_lut_is_valid(lut));
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

TEST_CASE("swtcon_update: rect that clamps to invalid (x0>x1) is dropped", "[swtcon]") {
  SwtconFixture fx;
  swtcon_lock();
  update_data req = rect_req(0, 100, 0, 50, 2, 9); // x0=100 > x1=50: inverted
  swtcon_update(&req);

  auto* queue = update_queue_globals();
  CHECK(queue->accumList.empty());
  swtcon_unlock_post();
}

TEST_CASE("swtcon_update: ExplicitTemperature flag uses the caller-supplied temperature",
          "[swtcon]") {
  SwtconFixture fx;
  swtcon_lock();
  update_data req{ 0, 0, 100, 100, ExplicitTemperature, 2, 10, 9 }; // zero=10 (explicit temp)
  swtcon_update(&req);
  swtcon_unlock_post();

  auto* queue = update_queue_globals();
  REQUIRE(queue->listIncomingUpdates.size() == 1);
  REQUIRE(queue->listIncomingUpdates.front().subList.size() == 1);
  CHECK(queue->listIncomingUpdates.front().subList.front().temperature == 10.0f);
}

TEST_CASE("swtcon_update: subtracts overlapping region from an already-batched "
          "(unclaimed) update", "[swtcon]") {
  SwtconFixture fx;
  swtcon_lock();
  update_data r1 = rect_req(200, 200, 1600, 1200, 2, 9);
  swtcon_update(&r1);
  swtcon_unlock_post(); // moves accumList into listIncomingUpdates as one batch

  swtcon_lock();
  update_data punch = rect_req(600, 500, 1200, 800, 2, 9);
  swtcon_update(&punch); // must also subtract from the already-queued batch above
  swtcon_unlock_post();

  auto* queue = update_queue_globals();
  REQUIRE(queue->listIncomingUpdates.size() == 2);
  CHECK(queue->listIncomingUpdates.front().subList.size() == 4); // r1 split into 4 pieces
  CHECK(queue->listIncomingUpdates.back().subList.size() == 1);  // punch's own batch
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

// --- swtcon.cpp: debug dump helpers + save_statebuffer/shutdown edge cases ---

TEST_CASE("swtcon_runtime_offset: reports zero when SWTCON_LIBIMPL isn't compiled in "
          "(non-armv7 host)", "[swtcon]") {
  CHECK(swtcon_runtime_offset() == 0);
}

TEST_CASE("swtcon_wait: returns immediately when nothing is queued", "[swtcon]") {
  SwtconFixture fx;
  REQUIRE_NOTHROW(swtcon_wait());
}

TEST_CASE("swtcon_init: no waveform override falls through to find_waveform_path, "
          "which fails on this host", "[swtcon]") {
  int fd = swtcon_test::make_anon_fd();
  REQUIRE(fd >= 0);
  InitParams params;
  params.skipPidLock = true;
  params.framebufferFd = fd;
  params.startThreads = false;
  // No waveformPathOverride - there's no real device path/barcode here for
  // find_waveform_path to match against.
  CHECK(swtcon_init(params) == nullptr);
}

TEST_CASE("swtcon_init: a bad waveform path override fails via load_waveform", "[swtcon]") {
  int fd = swtcon_test::make_anon_fd();
  REQUIRE(fd >= 0);
  InitParams params;
  params.skipPidLock = true;
  params.framebufferFd = fd;
  params.startThreads = false;
  params.waveformPathOverride = "/no/such/waveform.wbf";
  CHECK(swtcon_init(params) == nullptr);
}

TEST_CASE("swtcon_dump_waveform / swtcon_dump_buffers: debug dumps run without crashing",
          "[swtcon]") {
  SwtconFixture fx;
  REQUIRE_NOTHROW(swtcon_dump_waveform());
  REQUIRE_NOTHROW(swtcon_dump_buffers());
}

TEST_CASE("save_statebuffer: writes the persisted statebuffer to the given path", "[swtcon]") {
  SwtconFixture fx;

  // save_statebuffer takes the destination path as a raw pointer-sized int
  // (uintptr_t), mirroring the original 32-bit ARM ABI - lossless on this
  // 64-bit host too now, so an ordinary stack/heap address works directly.
  std::string path =
    (std::filesystem::temp_directory_path() / "swtcon-test-save-statebuffer.bin").string();

  auto* sb = statebuffer_globals();
  std::vector<uint8_t> expected((const uint8_t*)sb->pStatebuffer,
                                 (const uint8_t*)sb->pStatebuffer + kStatebufferSize);

  save_statebuffer((uintptr_t)path.c_str());

  std::ifstream saved(path, std::ios::binary);
  REQUIRE(saved.is_open());
  std::vector<uint8_t> actual(kStatebufferSize);
  saved.read((char*)actual.data(), (std::streamsize)kStatebufferSize);
  CHECK((size_t)saved.gcount() == kStatebufferSize);
  CHECK(actual == expected); // the real content, not just the right size
  saved.close();
  std::filesystem::remove(path);
}

TEST_CASE("swtcon_shutdown: non-zero state pointer saves state, and blanks an unblanked panel",
          "[swtcon]") {
  int fd = swtcon_test::make_anon_fd();
  REQUIRE(fd >= 0);
  std::string wbfPath = swtcon_test::test_wbf_path().string();

  InitParams params;
  params.skipPidLock = true;
  params.waveformPathOverride = wbfPath.c_str();
  params.framebufferFd = fd;
  params.startThreads = false;
  REQUIRE(swtcon_init(params) != nullptr);

  // Real hardware/ioctls always fail against the fake fd, so nIsFbBlanked
  // otherwise never leaves "blanked" - force it here so shutdown's
  // is_fb_blanked()==0 branch (-> blank_fb()) actually runs.
  framebuffer_globals()->nIsFbBlanked = 0;

  std::string path =
    (std::filesystem::temp_directory_path() / "swtcon-test-shutdown-state.bin").string();

  // Snapshot before shutdown - pStatebuffer is freed as part of teardown, so
  // it can't be read back afterward.
  auto* sb = statebuffer_globals();
  std::vector<uint8_t> expected((const uint8_t*)sb->pStatebuffer,
                                 (const uint8_t*)sb->pStatebuffer + kStatebufferSize);

  swtcon_shutdown((uintptr_t)path.c_str());

  std::ifstream saved(path, std::ios::binary);
  REQUIRE(saved.is_open());
  std::vector<uint8_t> actual(kStatebufferSize);
  saved.read((char*)actual.data(), (std::streamsize)kStatebufferSize);
  CHECK((size_t)saved.gcount() == kStatebufferSize);
  CHECK(actual == expected); // the real content, not just the right size
  saved.close();
  std::filesystem::remove(path);
}

// --- display.cpp: commit_item / dispatch_processed_regions_native (per-item
// state-buffer diffing + pixelTransitions packing) ---

TEST_CASE("commit_item: force path writes every pixel unconditionally", "[swtcon]") {
  SwtconFixture fx;
  WorkItem item = make_rendered_item(100, 100, 107, 101, /*sync=*/1, /*fillValue=*/50);

  REQUIRE(commit_item(&item));
  CHECK(item.rectY0 == 100); // force path never narrows
  CHECK(item.rectY1 == 107);
  CHECK(item.pixelTransitions != nullptr);
  CHECK(item.transitionDataPtr != nullptr);

  auto* state = (uint16_t*)statebuffer_globals()->pStatebuffer;
  CHECK(state[(size_t)100 * kScreenHeight + 100] == 50);

  // The actual packed transition value (old<<kPixelTransitionShift | new),
  // not just that the buffer got allocated - old is the neutral fill (30),
  // new is fillValue (50). rect is 8 rows x 2 cols, strideRows=16, so
  // (row=100,col=100) is local (0,0) -> index 0, and (row=107,col=101) is
  // local (7,1) -> index 16*1+7=23.
  auto* transitions = (const uint16_t*)item.transitionDataPtr;
  uint16_t expectedPacked = (uint16_t)((30 << kPixelTransitionShift) | 50);
  CHECK(transitions[0] == expectedPacked);
  CHECK(transitions[23] == expectedPacked);
}

TEST_CASE("commit_item: incremental path", "[swtcon]") {
  SwtconFixture fx;

  SECTION("no change anywhere leaves the item degenerate") {
    // The neutral statebuffer fill is 0x1e (30) per pixel.
    WorkItem item = make_rendered_item(200, 200, 207, 201, /*sync=*/0, /*fillValue=*/30);
    CHECK_FALSE(commit_item(&item));
  }

  SECTION("gated (sentinel) pixels don't count as a change") {
    WorkItem item =
      make_rendered_item(200, 200, 207, 201, /*sync=*/0, /*fillValue=*/(uint8_t)kGatedPixelSentinel);
    CHECK_FALSE(commit_item(&item));
  }

  SECTION("a real change survives and reports its rect back") {
    WorkItem item = make_rendered_item(200, 200, 207, 201, /*sync=*/0, /*fillValue=*/50);
    REQUIRE(commit_item(&item));
    CHECK(item.rectY0 == 200);
    CHECK(item.rectY1 == 207);
    CHECK(item.rectX0 == 200);
    CHECK(item.rectX1 == 201);
  }
}

TEST_CASE("commit_item: a single changed pixel widens the narrowed rect to its whole "
          "8-row group", "[swtcon]") {
  SwtconFixture fx;
  WorkItem item{};
  item.rectY0 = 300;
  item.rectX0 = 300;
  item.rectY1 = 307;
  item.rectX1 = 300; // 8 rows, 1 col
  item.sync = 0;

  int stride = 16; // round_up(8, 16)
  auto* rr = new RegionRows{};
  rr->y0 = 300;
  rr->x0 = 300;
  rr->y1 = 307;
  rr->x1 = 300;
  rr->stride = stride;
  rr->size = stride;
  rr->dataPtr = new uint8_t[stride];
  std::fill_n(rr->dataPtr, stride, (uint8_t)30); // neutral everywhere...
  rr->dataPtr[3] = 50;                           // ...except local row 3 (absolute row 303)
  item.regionRows = std::shared_ptr<RegionRows>(rr, [](RegionRows* p) {
    delete[] p->dataPtr;
    delete p;
  });
  item.pixelDataPtr = (uintptr_t)rr->dataPtr;

  REQUIRE(commit_item(&item));
  CHECK(item.rectY0 == 300);
  CHECK(item.rectY1 == 307); // whole group widened even though only row 303 changed
  CHECK(item.rectX0 == 300);
  CHECK(item.rectX1 == 300);
}

TEST_CASE("dispatch_processed_regions_native: erases degenerate/unchanged items, "
          "keeps survivors", "[swtcon]") {
  SwtconFixture fx;
  std::list<WorkItem> sub_list;

  WorkItem degenerate{};
  degenerate.rectY0 = 5;
  degenerate.rectY1 = 2; // y1 < y0: degenerate on entry
  sub_list.push_back(std::move(degenerate));

  sub_list.push_back(make_rendered_item(400, 400, 407, 401, /*sync=*/1, /*fillValue=*/60));

  bool any_survived = dispatch_processed_regions_native(sub_list);
  CHECK(any_survived);
  REQUIRE(sub_list.size() == 1);
  CHECK(sub_list.front().rectY0 == 400);
}

TEST_CASE("stale_row_cleanup: restores + clears dirty-gate rows that fall out of the "
          "live window", "[swtcon]") {
  SwtconFixture fx;
  auto* cursor = frame_cursor_globals();
  cursor->nFrameCleanupCursor = 0;
  cursor->nLastPannedFrame = 0;

  uint8_t* gate = backbuffer_dirty_gate();
  gate[0] = 1; // mark column 0 of bucket 0 dirty

  // copy_init_frame_row's actual restore target for (bucket=0, col=0):
  // frame_buffer_addr(0) + (col+3)*0x410, 0x410 bytes, copied from
  // fb->pLUT+0xc30 - seed it with a sentinel first so a no-op "restore"
  // (dirty-gate cleared but nothing actually copied) would be caught.
  auto* dest = (uint8_t*)frame_buffer_addr(0) + 0xc30;
  std::fill_n(dest, 0x410, (uint8_t)0xAA);
  auto* src = (const uint8_t*)framebuffer_globals()->pLUT + 0xc30;

  REQUIRE_NOTHROW(stale_row_cleanup());
  CHECK(gate[0] == 0);                                  // cleared
  CHECK(cursor->nFrameCleanupCursor == kFrameSlotRingCount); // advanced by one bucket
  CHECK(std::memcmp(dest, src, 0x410) == 0); // the dirty row was actually restored
}

TEST_CASE("gc_processed_updates: expires lifetime-elapsed items and scrubs dangling deps",
          "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  WorkItem expiring{};
  expiring.frameAnchor = 0;
  expiring.lutWidthMinus1 = 5; // expires once curFrame >= 5
  queue->listProcessedUpdates.push_back(expiring);
  WorkItem* expiringPtr = &queue->listProcessedUpdates.back();

  WorkItem survivor{};
  survivor.frameAnchor = 0;
  survivor.lutWidthMinus1 = 100; // still alive
  survivor.deps.push_back(expiringPtr);
  queue->listProcessedUpdates.push_back(survivor);

  queue->curFrame = 5; // >= expiring's frameAnchor(0) + lutWidthMinus1(5)

  gc_processed_updates();

  REQUIRE(queue->listProcessedUpdates.size() == 1);
  CHECK(queue->listProcessedUpdates.front().lutWidthMinus1 == 100);
  CHECK(queue->listProcessedUpdates.front().deps.empty()); // dangling dep scrubbed
}

TEST_CASE("build_overlap_dependency_list: links only overlapping, still-live prior items",
          "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  WorkItem stillActive = make_item(0, 0, 100, 100);
  stillActive.frameAnchor = 10;
  stillActive.lutWidthMinus1 = 50; // alive through frame 60
  queue->listProcessedUpdates.push_back(stillActive);

  WorkItem nonOverlapping = make_item(500, 500, 600, 600);
  nonOverlapping.frameAnchor = 10;
  nonOverlapping.lutWidthMinus1 = 50;
  queue->listProcessedUpdates.push_back(nonOverlapping);

  WorkItem alreadyExpired = make_item(0, 0, 100, 100);
  alreadyExpired.frameAnchor = 0;
  alreadyExpired.lutWidthMinus1 = 5; // dead well before the new item's frameAnchor below
  queue->listProcessedUpdates.push_back(alreadyExpired);

  std::list<WorkItem> sub_list;
  WorkItem newItem = make_item(50, 50, 150, 150); // overlaps stillActive/alreadyExpired
  newItem.frameAnchor = 20;                       // > alreadyExpired's frameAnchor+lutWidthMinus1
  sub_list.push_back(newItem);

  build_overlap_dependency_list(sub_list);

  REQUIRE(sub_list.size() == 1);
  auto& deps = sub_list.front().deps;
  REQUIRE(deps.size() == 1);
  CHECK(deps.front()->lutWidthMinus1 == 50); // only `stillActive` tracked
}

TEST_CASE("build_overlap_dependency_list: skips degenerate rects on either side",
          "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  WorkItem degenerateOther = make_item(0, 0, -1, -1); // other.rectY1 < other.rectY0
  queue->listProcessedUpdates.push_back(degenerateOther);

  std::list<WorkItem> sub_list;
  WorkItem degenerateNew = make_item(5, 5, 2, 2); // item's own rect degenerate
  sub_list.push_back(degenerateNew);
  sub_list.push_back(make_item(0, 0, 100, 100)); // valid, but only overlaps the degenerate other

  build_overlap_dependency_list(sub_list);

  REQUIRE(sub_list.size() == 2);
  CHECK(sub_list.front().deps.empty()); // degenerate item skipped before any scan
  CHECK(sub_list.back().deps.empty());  // valid item, but the only prior item is degenerate
}

TEST_CASE("playback_chunk_count: 1 unless area exceeds 20000px and columns are wide enough",
          "[swtcon]") {
  WorkItem item = make_item(5, 0, 2, 0); // degenerate
  CHECK(playback_chunk_count(&item) == 1);

  item = make_item(0, 0, 99, 99); // 100x100 = 10000px, under the threshold
  CHECK(playback_chunk_count(&item) == 1);

  item = make_item(0, 0, 2500, 8); // area=9*2501>20000, width_span=8<10
  CHECK(playback_chunk_count(&item) == 1);

  item = make_item(0, 0, 2500, 10); // area=11*2501>20000, width_span=10>=10
  CHECK(playback_chunk_count(&item) == 2);
}

// Builds its work item via make_rendered_item rather than swtcon_update's
// own dispatch_update_regions, since these use the force path (sync=1)
// where the exact rendered pixel content is irrelevant - only that
// commit_item can read it back at all.

TEST_CASE("advance_work_item_frames: aligned kernel path advances phase/frameCursor "
          "when no active dependency exists", "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  WorkItem item = make_rendered_item(0, 0, 63, 63, /*sync=*/1, /*fillValue=*/50);
  item.pixelMode = 9;
  item.mode = 2;
  item.temperature = 25.0f;

  select_waveform_lut(item.temperature, &item.lut, &queue->waveform, (unsigned)item.mode);
  REQUIRE(update_lut_is_valid(item.lut));
  auto* lutWords = (const int32_t*)item.lut.get();
  item.lutWidthMinus1 = (int16_t)(lutWords[0] - 1);
  REQUIRE(item.lutWidthMinus1 > 0);

  std::list<WorkItem> sub_list;
  sub_list.push_back(std::move(item));

  build_overlap_dependency_list(sub_list); // no prior processed items -> empty deps
  REQUIRE(dispatch_processed_regions_native(sub_list));
  REQUIRE(sub_list.size() == 1);

  WorkItem& committed = sub_list.front();
  committed.frameCursor = 0;
  committed.frameAnchor = 0;
  committed.phase = 0;
  // Plenty of cleanup-cursor budget so the kernel actually dispatches (see
  // advance_work_item_frames's `budget` bound).
  frame_cursor_globals()->nFrameCleanupCursor = 1000;

  advance_work_item_frames(&committed);

  CHECK(committed.frameCursor > 0);
  CHECK(committed.phase > 0);
  CHECK(queue->targetFrame == committed.frameCursor);
}

TEST_CASE("advance_work_item_frames: plain-kernel path aborts when the dependency "
          "bound leaves no budget", "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  WorkItem item = make_rendered_item(0, 0, 63, 63, /*sync=*/1, /*fillValue=*/50);
  item.pixelMode = 9;
  item.mode = 2;
  item.temperature = 25.0f;

  select_waveform_lut(item.temperature, &item.lut, &queue->waveform, (unsigned)item.mode);
  REQUIRE(update_lut_is_valid(item.lut));
  auto* lutWords = (const int32_t*)item.lut.get();
  item.lutWidthMinus1 = (int16_t)(lutWords[0] - 1);

  // A still-active prior item covering the same rect: gives the new item an
  // active dependency, forcing the "plain" kernel selection.
  WorkItem stillActive = make_item(0, 0, 63, 63);
  stillActive.frameAnchor = 0;
  stillActive.lutWidthMinus1 = 1000; // outlives the new item's own frameAnchor
  queue->listProcessedUpdates.push_back(stillActive);

  std::list<WorkItem> sub_list;
  sub_list.push_back(std::move(item));
  build_overlap_dependency_list(sub_list); // links the new item to stillActive
  REQUIRE(dispatch_processed_regions_native(sub_list));
  REQUIRE(sub_list.size() == 1);

  WorkItem& committed = sub_list.front();
  committed.frameCursor = 0;
  committed.frameAnchor = 0;
  committed.phase = 0;
  frame_cursor_globals()->nFrameCleanupCursor = 1000;

  advance_work_item_frames(&committed);

  // stillActive's own frameCursor (0) bounds the dependent bound to 0 -
  // the plain-kernel path aborts entirely rather than dispatching.
  CHECK(committed.frameCursor == 0);
  CHECK(committed.phase == 0);
}

TEST_CASE("advance_work_item_frames: plain kernel path actually dispatches when "
          "phase is mid-alignment", "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  WorkItem item = make_rendered_item(0, 0, 63, 63, /*sync=*/1, /*fillValue=*/50);
  item.pixelMode = 9;
  item.mode = 2;
  item.temperature = 25.0f;

  select_waveform_lut(item.temperature, &item.lut, &queue->waveform, (unsigned)item.mode);
  REQUIRE(update_lut_is_valid(item.lut));
  auto* lutWords = (const int32_t*)item.lut.get();
  item.lutWidthMinus1 = (int16_t)(lutWords[0] - 1);
  REQUIRE(item.lutWidthMinus1 > 8); // enough headroom for a mid-alignment phase

  std::list<WorkItem> sub_list;
  sub_list.push_back(std::move(item));
  build_overlap_dependency_list(sub_list); // no prior processed items -> empty deps
  REQUIRE(dispatch_processed_regions_native(sub_list));

  WorkItem& committed = sub_list.front();
  committed.frameCursor = 0;
  committed.frameAnchor = 0;
  committed.phase = 1; // not 8-aligned -> forces the "plain" kernel path even with no deps
  frame_cursor_globals()->nFrameCleanupCursor = 1000;

  advance_work_item_frames(&committed);

  CHECK(committed.frameCursor > 0);
  CHECK(committed.phase > 1);
}

TEST_CASE("advance_work_item_frames: marks the backbuffer dirty-gate columns for "
          "each newly-advanced frame slot", "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  // A column range no other test touches - backbuffer_dirty_gate() is a
  // process-wide static that outlives any single SwtconFixture, so reusing
  // a range another test already marked dirty would make this test's own
  // "clean before" precondition unreliable.
  WorkItem item = make_rendered_item(0, 900, 63, 910, /*sync=*/1, /*fillValue=*/50);
  item.pixelMode = 9;
  item.mode = 2;
  item.temperature = 25.0f;

  select_waveform_lut(item.temperature, &item.lut, &queue->waveform, (unsigned)item.mode);
  REQUIRE(update_lut_is_valid(item.lut));
  auto* lutWords = (const int32_t*)item.lut.get();
  item.lutWidthMinus1 = (int16_t)(lutWords[0] - 1);
  REQUIRE(item.lutWidthMinus1 > 0);

  std::list<WorkItem> sub_list;
  sub_list.push_back(std::move(item));
  build_overlap_dependency_list(sub_list);
  REQUIRE(dispatch_processed_regions_native(sub_list));

  WorkItem& committed = sub_list.front();
  committed.frameCursor = 0;
  committed.frameAnchor = 0;
  committed.phase = 0;
  frame_cursor_globals()->nFrameCleanupCursor = 1000;

  uint8_t* bucket0 = backbuffer_dirty_gate(); // start_frame_cursor(0) % kFrameSlotRingCount == 0
  REQUIRE(bucket0[900] == 0);                 // clean before - see the column-range note above
  REQUIRE(bucket0[910] == 0);

  advance_work_item_frames(&committed);

  REQUIRE(committed.frameCursor > 0); // sanity: the kernel actually dispatched

  // advance_work_item_frames's own dirty-gate *producer* step - the
  // "stale_row_cleanup" test above only ever exercises the *consumer* side,
  // by poking the gate directly rather than through a real advance. Every
  // column across [rectX0, rectX1] should now be marked dirty in the bucket
  // this call advanced through, and nothing outside that range touched.
  CHECK(bucket0[900] == 1);
  CHECK(bucket0[905] == 1);
  CHECK(bucket0[910] == 1);
  CHECK(bucket0[899] == 0);
  CHECK(bucket0[911] == 0);
}

TEST_CASE("advance_work_item_frames: aligned path with insufficient budget falls "
          "through without dispatching, warning if the generator has fallen behind",
          "[swtcon]") {
  SwtconFixture fx;
  auto* queue = update_queue_globals();

  WorkItem item = make_rendered_item(0, 0, 63, 63, /*sync=*/1, /*fillValue=*/50);
  item.pixelMode = 9;
  item.mode = 2;
  item.temperature = 25.0f;

  select_waveform_lut(item.temperature, &item.lut, &queue->waveform, (unsigned)item.mode);
  REQUIRE(update_lut_is_valid(item.lut));
  auto* lutWords = (const int32_t*)item.lut.get();
  item.lutWidthMinus1 = (int16_t)(lutWords[0] - 1);

  std::list<WorkItem> sub_list;
  sub_list.push_back(std::move(item));
  build_overlap_dependency_list(sub_list);
  REQUIRE(dispatch_processed_regions_native(sub_list));

  WorkItem& committed = sub_list.front();
  committed.frameCursor = 0;
  committed.frameAnchor = 0;
  committed.phase = 0; // 8-aligned + no deps -> the aligned path is selected
  // nFrameCleanupCursor left at its post-init default (0): budget = 1, almost
  // certainly less than frame_count, so the aligned path skips dispatching.
  queue->curFrame = 100; // simulate the generator thread having fallen behind

  advance_work_item_frames(&committed);

  CHECK(committed.frameCursor == 0); // nothing dispatched
  CHECK(queue->targetFrame == 0);    // early-returned before broadcasting
}

// --- playback_kernel_intrinsics.cpp: playback_kernel_plain/aligned_intrinsics
// (per-column/per-8-row-group LUT playback into frame slots; KERNEL_MODE_C
// on this non-ARM host) ---

TEST_CASE("playback_kernel_plain_intrinsics: degenerate/empty rects are a no-op", "[swtcon]") {
  WorkItem item{};
  void* frame_slots[1] = { nullptr };

  item.rectY0 = 5;
  item.rectY1 = 2; // y1 < y0
  item.rectX0 = 0;
  item.rectX1 = 0;
  REQUIRE_NOTHROW(playback_kernel_plain_intrinsics(frame_slots, &item, 1, 0, 1));

  item.rectY0 = 0;
  item.rectY1 = 2; // span < 8 rows -> num_groups == 0
  REQUIRE_NOTHROW(playback_kernel_plain_intrinsics(frame_slots, &item, 1, 0, 1));
}

TEST_CASE("playback_kernel_plain_intrinsics: transposes 8 LUT rows into per-subphase "
          "words and ORs them into up to frame_count frame slots", "[swtcon]") {
  WorkItem item{};
  item.rectY0 = 0;
  item.rectX0 = 0;
  item.rectY1 = 7;
  item.rectX1 = 0; // 1 col, one 8-row group
  item.phase = 0;

  auto lut = std::make_shared<LUTEntry>();
  lut->mode_width = 1;
  auto* lut_data = (uint16_t*)malloc(sizeof(uint16_t));
  lut_data[0] = 0xffff; // every 2-bit field is 0b11
  lut->data = lut_data; // freed by ~LUTEntry() via free()
  item.lut = lut;

  auto rr = std::make_shared<RegionRows>();
  rr->stride = 1;
  item.pixelTransitions = rr;

  uint16_t transitions[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; // all rows -> lut word index 0
  item.transitionDataPtr = transitions;

  std::vector<uint8_t> frame_buf[8];
  void* frame_slots[8];
  for (int i = 0; i < 8; i++) {
    frame_buf[i].assign(8192, 0);
    frame_slots[i] = frame_buf[i].data();
  }

  playback_kernel_plain_intrinsics(frame_slots, &item, /*frame_count=*/3, 0, 1);

  size_t byte_off = (size_t)((0 + 3) * 0x104 + (0 >> 3) + 0 + 0x1a) * 4;
  for (int k = 0; k < 3; k++) {
    auto* dest = (uint16_t*)(frame_buf[k].data() + byte_off);
    CHECK(*dest == 0xffff);
  }
  for (int k = 3; k < 8; k++) {
    auto* dest = (uint16_t*)(frame_buf[k].data() + byte_off);
    CHECK(*dest == 0);
  }
}

TEST_CASE("playback_kernel_aligned_intrinsics: delegates to the plain kernel on this host",
          "[swtcon]") {
  WorkItem item{};
  item.rectY0 = 5;
  item.rectY1 = 2; // degenerate, safe no-op
  item.rectX0 = 0;
  item.rectX1 = 0;
  void* frame_slots[1] = { nullptr };
  REQUIRE_NOTHROW(playback_kernel_aligned_intrinsics(frame_slots, &item, 1, 0, 1));
}

// --- Mocked /dev/fb0 + ioctl (MockFb.h) - unlike every SwtconFixture-based
// test above (where every fb ioctl fails outright against the bare memfd),
// these let pan_and_unblank/blank_fb actually succeed, so the worker/display
// threads' real hardware-touching steps run for real instead of just their
// skeletons. ---

namespace {
FbInitParams
make_fb_init_params() {
  FbInitParams fb_info{};
  fb_info.xres = 0x104;
  fb_info.yres = 0x580;
  fb_info.bitsPerPixel = 0x20;
  fb_info.pixclock = 0x7080;
  fb_info.frameCount = kFrameSlotRingCount;
  fb_info.leftMargin = 1;
  fb_info.rightMargin = 1;
  fb_info.upperMargin = 1;
  fb_info.lowerMargin = 0x8f;
  fb_info.hsyncLen = 1;
  fb_info.vsyncLen = 1;
  return fb_info;
}
} // namespace

TEST_CASE("init_framebuffer: succeeds against a mocked /dev/fb0", "[swtcon]") {
  swtcon_test::MockFramebuffer mockFb;

  FbInitParams params = make_fb_init_params();
  REQUIRE(init_framebuffer(params) == 0);
  auto* fb = framebuffer_globals();
  CHECK(fb->pFbMmap != nullptr);
  CHECK(fb->nFbSizeX > 0);
  CHECK(fb->nFbSizeY == kFrameSlotRingCount + 1);

  // fill_var_screeninfo's actual field-by-field output, sent to the kernel
  // via the (mocked) FBIOPUT_VSCREENINFO - a wrong/zeroed field here is
  // exactly what makes the real driver reject the pan ioctl on hardware
  // (see fill_var_screeninfo's own comment).
  auto& vinfo = fb->fbVar;
  CHECK(vinfo.xres == (uint32_t)params.xres);
  CHECK(vinfo.yres == (uint32_t)params.yres);
  CHECK(vinfo.xres_virtual == (uint32_t)params.xres);
  CHECK(vinfo.yres_virtual == (uint32_t)params.yres * (params.frameCount + 1));
  CHECK(vinfo.yoffset == (uint32_t)params.frameCount * params.yres);
  CHECK(vinfo.bits_per_pixel == (uint32_t)params.bitsPerPixel);
  CHECK(vinfo.pixclock == (uint32_t)params.pixclock);
  CHECK(vinfo.left_margin == (uint32_t)params.leftMargin);
  CHECK(vinfo.right_margin == (uint32_t)params.rightMargin);
  CHECK(vinfo.upper_margin == (uint32_t)params.upperMargin);
  CHECK(vinfo.lower_margin == (uint32_t)params.lowerMargin);
  CHECK(vinfo.hsync_len == (uint32_t)params.hsyncLen);
  CHECK(vinfo.vsync_len == (uint32_t)params.vsyncLen);

  close_fb(); // munmaps + closes what init_framebuffer just opened
}

TEST_CASE("init_framebuffer: reports failure at every real-hardware error site",
          "[swtcon]") {
  swtcon_test::MockFramebuffer mockFb;

  SECTION("device open failure") {
    swtcon_test::MockFramebuffer::failNextOpen();
    CHECK(init_framebuffer(make_fb_init_params()) == -1);
  }

  SECTION("FBIOGET_FSCREENINFO failure") {
    swtcon_test::MockFramebuffer::failNextIoctl(FBIOGET_FSCREENINFO);
    CHECK(init_framebuffer(make_fb_init_params()) == -1);
  }

  SECTION("FBIOGET_VSCREENINFO failure") {
    swtcon_test::MockFramebuffer::failNextIoctl(FBIOGET_VSCREENINFO);
    CHECK(init_framebuffer(make_fb_init_params()) == -1);
  }

  SECTION("FBIOPUT_VSCREENINFO failure") {
    swtcon_test::MockFramebuffer::failNextIoctl(FBIOPUT_VSCREENINFO);
    CHECK(init_framebuffer(make_fb_init_params()) == -1);
  }
}

TEST_CASE("pan_and_unblank / blank_fb: succeed against a mocked framebuffer fd",
          "[swtcon]") {
  SwtconFixture fx; // startThreads=false; wires framebuffer_globals() up to fx.fd
  swtcon_test::MockFramebuffer mockFb;
  swtcon_test::MockFramebuffer::registerFd(fx.fd);

  framebuffer_globals()->nIsFbBlanked = 1; // pan_and_unblank's own precondition
  REQUIRE(pan_and_unblank(0) == 0);
  CHECK(framebuffer_globals()->nIsFbBlanked == 0);

  auto st = swtcon_test::MockFramebuffer::state(fx.fd);
  CHECK(st.unblankCount >= 1);
  CHECK_FALSE(st.blanked);

  blank_fb();
  CHECK(framebuffer_globals()->nIsFbBlanked == 1);
  st = swtcon_test::MockFramebuffer::state(fx.fd);
  CHECK(st.blankCount >= 1);
  CHECK(st.blanked);
}

TEST_CASE("pan_and_advance_frame: pans to curFrame % kFrameSlotRingCount, not a fixed slot",
          "[swtcon]") {
  SwtconFixture fx; // startThreads=false; wires framebuffer_globals() up to fx.fd
  swtcon_test::MockFramebuffer mockFb;
  swtcon_test::MockFramebuffer::registerFd(fx.fd);

  auto* queue = update_queue_globals();
  auto* cursor = frame_cursor_globals();
  int32_t yres = (int32_t)framebuffer_globals()->fbVar.yres;

  // Past one full ring lap, so a wraparound bug (e.g. always slot 0) can't
  // hide behind curFrame's own initial value.
  queue->curFrame = kFrameSlotRingCount + 3;
  int32_t expectedFrameIdx = (kFrameSlotRingCount + 3) % kFrameSlotRingCount;

  pan_and_advance_frame(queue, cursor, /*unblank=*/false);

  auto st = swtcon_test::MockFramebuffer::state(fx.fd);
  CHECK(st.panCount >= 1);
  CHECK(st.lastYoffset == expectedFrameIdx * yres); // the actual slot panned to

  // nLastPannedFrame is stamped to curFrame-1 (the frame just panned to,
  // before this call's own increment) - see this function's own comment for
  // why that's not an off-by-one.
  CHECK(cursor->nLastPannedFrame == kFrameSlotRingCount + 2);
  CHECK(queue->curFrame == kFrameSlotRingCount + 4);
}

TEST_CASE("swtcon threads: a queued update is processed and drives real pan/blank "
          "activity through the worker/display threads", "[swtcon]") {
  swtcon_test::MockFramebuffer mockFb;
  int fd = swtcon_test::make_anon_fd();
  REQUIRE(fd >= 0);
  // Registered before swtcon_init() so its own internal prime_display() (the
  // startup pan/unblank/reblank sequence) succeeds for real too, not just
  // the update this test queues below.
  swtcon_test::MockFramebuffer::registerFd(fd);

  std::string wbfPath = swtcon_test::test_wbf_path().string();
  InitParams params;
  params.skipPidLock = true;
  params.waveformPathOverride = wbfPath.c_str();
  params.framebufferFd = fd;
  params.startThreads = true;
  REQUIRE(swtcon_init(params) != nullptr);

  swtcon_lock();
  update_data req{ 0, 0, 800, 600, Sync, /*mode=*/2, 0, /*pixel_mode=*/9 };
  swtcon_update(&req);
  swtcon_unlock_post();

  // swtcon_wait() only guarantees the incoming batch was claimed off
  // listIncomingUpdates (display_thread_func's own intake step, which also
  // runs the item through commit_item/advance_work_item_frames
  // synchronously within that same tick) - poll briefly afterward for the
  // worker thread's own next tick to actually issue the resulting real
  // pan/blank ioctls, rather than assuming a fixed delay is enough.
  bool drained = run_with_timeout([&] { swtcon_wait(); }, std::chrono::seconds(5));
  REQUIRE(drained);

  auto* queue = update_queue_globals();
  bool committed = false;
  for (int i = 0; i < 100 && !committed; i++) {
    if (!queue->listProcessedUpdates.empty()) {
      committed = true;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  CHECK(committed); // the item survived commit_item and is now playing back

  bool panned = false;
  for (int i = 0; i < 100 && !panned; i++) {
    if (swtcon_test::MockFramebuffer::state(fd).panCount > 0) {
      panned = true;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  CHECK(panned); // the worker thread issued a real (mocked) pan ioctl

  bool finished = run_with_timeout([] { swtcon_shutdown(0); }, std::chrono::seconds(5));
  CHECK(finished);
}
