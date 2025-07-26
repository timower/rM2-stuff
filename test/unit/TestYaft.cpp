#include <catch2/catch_test_macros.hpp>

#include "rMLibTestHelper.h"

#include "YaftWidget.h"

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
      const FindResult<RO>& ros,
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
       const FindResult<RO>& ros,
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
    .repeatDelay = std::chrono::milliseconds(50),
    .repeatTime = std::chrono::milliseconds(10),
  };

  ctx.pumpWidget(Center(Sized(Keyboard(nullptr, params, updateCb), 300, 200)));

  auto kbd = ctx.findByType<Keyboard>();

  REQUIRE_THAT(kbd, ctx.matchesGolden("yaft-keyboard.png"));

  // Test key feedback when pressing.
  doKey(ctx, kbd, "a", true);
  ctx.pump(params.repeatDelay + 2 * params.repeatTime + params.repeatTime / 2);
  REQUIRE_THAT(kbd, ctx.matchesGolden("yaft-keyboard-down.png"));
  doKey(ctx, kbd, "a", false);
  REQUIRE(lastCallback == 1);

  // Make sure repeat works.
  CHECK(callbackCount == 3);

  // Also make sure a wide key gets updated correctly.
  doKey(ctx, kbd, "b", true);
  ctx.pump();
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
  ctx.pump(params.repeatDelay + params.repeatTime / 2);
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
  ctx.pump(params.repeatDelay + params.repeatTime / 2);
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
    cfgAndErr.config.orientation = YaftConfig::Orientation::Landscape;
    cfgAndErr.err = YaftConfigError{ YaftConfigError::Missing, "Error test!" };

    ctx.pumpWidget(Yaft(args.front(), args.data(), cfgAndErr));
    ctx.pump();

    auto yaft = ctx.findByType<Yaft>();
    REQUIRE_THAT(yaft, ctx.matchesGolden("yaft-landscape.png"));
  }

  SECTION("portrait") {
    cfgAndErr.config.orientation = YaftConfig::Orientation::Protrait;

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

    SECTION("Exit") {
      tap(":enter");
      tap("ctrl");
      tap("d");
      REQUIRE(ctx.shouldStop());
    }
  }
}
