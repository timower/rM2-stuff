#include <catch2/catch_test_macros.hpp>

#include "TempFiles.h"
#include "rMLibTestHelper.h"

#include "YaftWidget.h"
#include "terminal_adapter.h"

#include <fstream>

using namespace rmlib;

namespace {

const Layout test_layout = { {
  { { "a", makeCallback(1) },
    { "b", makeCallback(2), "", 0, /* width */ 2 },
    { "c", makeCallback(3) } },

  {
    { "ctrl", Ctrl },
    { "alt", Alt },
    { ":shift", Shift },
    { "<>", makeCallback(4), "^", makeCallback(5), 1, makeCallback(6) },
  },
} };

template<typename RO>
void
doKey(TestContext& ctx,
      const FindResults<RO>& ros,
      std::string_view name,
      bool down,
      const Layout& layout = test_layout) {

  bool found = false;
  int rowIdx = 0;
  int colIdx = 0;
  for (const auto& row : layout.rows) {
    for (const auto& key : row) {
      if (key.name == name) {
        found = true;
        break;
      }

      colIdx += key.width;
    }

    if (found) {
      break;
    }

    rowIdx++;
    colIdx = 0;
  }

  INFO("Key " << name << " not found");
  REQUIRE(found);

  float x = (float(colIdx) + 0.5f) / layout.numCols();
  float y = (float(rowIdx) + 0.5f) / layout.numRows();

  ctx.sendInput(down, ros, { x, y });
}

template<typename RO>
void
tapKey(TestContext& ctx,
       const FindResults<RO>& ros,
       std::string_view name,
       const Layout& layout = test_layout) {
  doKey(ctx, ros, name, true, layout);
  ctx.pump();
  doKey(ctx, ros, name, false, layout);
}

} // namespace

TEST_CASE("Keyboard", "[yaft][ui]") {
  auto ctx = TestContext::make();

  int lastCallback = -1;
  int callbackCount = 0;
  const auto updateCb = [&](auto cb) {
    if (lastCallback == cb) {
      callbackCount++;
    }
    lastCallback = cb;
  };

  const auto params = KeyboardParams{
    .layout = test_layout,
    .keymap = qwerty_keymap,
    .repeatDelay = std::chrono::milliseconds(100),
    .repeatTime = std::chrono::milliseconds(50),
  };

  constexpr auto width = 300;
  constexpr auto height = 200;
  ctx.pumpWidget(
    Center(Sized(Keyboard(nullptr, params, updateCb), width, height)));

  auto kbd = ctx.findByType<Keyboard>();

  REQUIRE_THAT(kbd, ctx.matchesGolden("yaft-keyboard.png"));

  // Test key feedback when pressing.
  doKey(ctx, kbd, "a", true);
  ctx.pump(params.repeatDelay + 3 * params.repeatTime);
  REQUIRE_THAT(kbd, ctx.matchesGolden("yaft-keyboard-down.png"));
  doKey(ctx, kbd, "a", false);
  REQUIRE(lastCallback == 1);

  // Make sure repeat works.
  CHECK(callbackCount >= 2);

  // Also make sure a wide key gets updated correctly.
  doKey(ctx, kbd, "b", true);
  ctx.pump(std::chrono::milliseconds(5));
  REQUIRE_THAT(kbd, ctx.matchesGolden("yaft-keyboard-down2.png"));
  doKey(ctx, kbd, "b", false);
  REQUIRE(lastCallback == 2);

  tapKey(ctx, kbd, "c");
  REQUIRE(lastCallback == 3);

  tapKey(ctx, kbd, ":shift");
  REQUIRE_THAT(kbd, ctx.matchesGolden("yaft-keyboard-held.png"));
  tapKey(ctx, kbd, "<>");
  REQUIRE(lastCallback == 5);

  tapKey(ctx, kbd, "<>");
  REQUIRE(lastCallback == 4);

  doKey(ctx, kbd, ":shift", true);
  ctx.pump(params.repeatDelay + params.repeatTime);
  REQUIRE_THAT(kbd, ctx.matchesGolden("yaft-keyboard-stuck.png"));
  doKey(ctx, kbd, ":shift", false);

  tapKey(ctx, kbd, "<>");
  REQUIRE(lastCallback == 5);

  tapKey(ctx, kbd, "<>");
  REQUIRE(lastCallback == 5);
  tapKey(ctx, kbd, ":shift");

  tapKey(ctx, kbd, "<>");
  REQUIRE(lastCallback == 4);

  doKey(ctx, kbd, "<>", true);
  ctx.pump(params.repeatDelay + params.repeatTime);
  doKey(ctx, kbd, "<>", false);
  REQUIRE(lastCallback == 6);
}

TEST_CASE("Yaft", "[yaft][ui]") {
  auto ctx = TestContext::make();

  std::string program = CAT_EXE;
  std::vector<char*> args = { program.data(), nullptr };

  YaftConfigAndError cfgAndErr;
  cfgAndErr.config = YaftConfig::getDefault();

  SECTION("landscape") {
    cfgAndErr.config.rotation = rmlib::Rotation::Clockwise;
    cfgAndErr.config.layout = &empty_layout;
    cfgAndErr.err = YaftConfigError{ YaftConfigError::Missing, "Error test!" };

    ctx.pumpWidget(Yaft(args.front(), args.data(), cfgAndErr));
    ctx.pump();

    auto yaft = ctx.findByType<Yaft>();
    REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-landscape.png"));
  }

  SECTION("portrait") {
    cfgAndErr.config.rotation = rmlib::Rotation::None;

    ctx.pumpWidget(Yaft(args.front(), args.data(), cfgAndErr));
    ctx.pump();

    auto yaft = ctx.findByType<Yaft>();
    auto kbd = ctx.findByType<Keyboard>();
    REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-init.png"));

    const auto down = [&](auto name) {
      doKey(ctx, kbd, name, true, qwerty_layout);
    };
    const auto up = [&](auto name) {
      doKey(ctx, kbd, name, false, qwerty_layout);
    };
    const auto tap = [&](auto name) { tapKey(ctx, kbd, name, qwerty_layout); };

    tap("a");
    tap(":shift");
    tap("a");
    tap(":enter");
    ctx.pump();
    REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-aA.png"));

    SECTION("Hide") {
      down("esc");
      ctx.pump(std::chrono::milliseconds(1200));
      up("esc");
      REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-hidden.png"));
    }

    // Regression test: moving the cursor onto blank rows with no other
    // content change must still erase its highlight from the row it left.
    // Ghostty's own dirty tracking never covers cursor position (only cell
    // content/palette/selection etc.), so this only passes if
    // ScreenRenderObject independently tracks the cursor's previous row.
    SECTION("Cursor redraw") {
      tap(":enter");
      tap(":enter");
      ctx.pump();
      REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-cursor-moved.png"));
    }

    SECTION("Exit") {
      tap(":enter");
      tap("ctrl");
      tap("d");
      REQUIRE(ctx.shouldStop());
    }
  }
}

// Regression test for the Terminal adapter's per-row dirty tracking (backed
// by libghostty-vt's native render-state dirty state): only rows that
// actually changed should come back dirty, not the whole screen.
TEST_CASE("Terminal dirty tracking", "[yaft]") {
  Terminal term(400, 800);

  auto feed = [&](const char* s) {
    term.parse(reinterpret_cast<const uint8_t*>(s), strlen(s));
  };
  auto dirtyRows = [&] {
    term.beginDraw();
    std::vector<int> rows;
    for (int i = 0; term.nextRow(); i++) {
      if (term.isRowDirty()) {
        rows.push_back(i);
      }
    }
    return rows;
  };
  auto clearAll = [&] {
    term.beginDraw();
    while (term.nextRow()) {
      term.clearRowDirty();
    }
  };

  INFO("cols=" << term.cols << " lines=" << term.lines);

  feed("hello\r\n");
  auto dirty1 = dirtyRows();
  CHECK_FALSE(dirty1.empty());
  clearAll();

  // Write far from row 0; row 0 must not show up dirty again, and the
  // number of dirty rows must be much smaller than the total row count.
  feed("\x1b[10;1Hworld");
  auto dirty2 = dirtyRows();
  CHECK_FALSE(dirty2.empty());
  CHECK(dirty2.size() < size_t(term.lines) / 2);
  CHECK(std::find(dirty2.begin(), dirty2.end(), 0) == dirty2.end());
  CHECK(std::find(dirty2.begin(), dirty2.end(), 9) != dirty2.end());
  clearAll();

  // No new writes: nothing should be dirty.
  auto dirty3 = dirtyRows();
  CHECK(dirty3.empty());
}

TEST_CASE("inotify", "[yaft][ui]") {
  TemporaryDirectory tmp;
  setenv("HOME", tmp.dir.c_str(), 1);
  setenv("XDG_CONFIG_HOME", "", 1);

  std::string program = CAT_EXE;
  std::vector<char*> args = { program.data(), nullptr };

  auto ctx = TestContext::make();
  ctx.pumpWidget(Yaft(args.front(), args.data(), getConfigPath()));

  auto yaft = ctx.findByType<Yaft>();
  REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-new.png"));

  std::ofstream cfgOfs(tmp.dir / ".config" / "yaft" / "config.toml");
  cfgOfs << "rotation = \"inverted\"";
  cfgOfs.close();

  ctx.pump(std::chrono::milliseconds(100));
  REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-inverted.png"));
}
