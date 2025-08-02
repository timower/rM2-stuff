#include <catch2/catch_test_macros.hpp>

#include "TempFiles.h"
#include "rMLibTestHelper.h"

#include "Calculator.h"

#include <UI/Navigator.h>

#include <cstdlib>

using namespace rmlib;
using namespace tilem;

TEST_CASE("Tilem", "[tilem][ui]") {
  TemporaryDirectory tmp;
  const auto romPath = tmp.dir / "unit_test.rom";

  auto ctx = TestContext::make();

  ctx.pumpWidget(Navigator(Calculator(romPath)));

  auto calc = ctx.findByType<Calculator>();

  REQUIRE_THAT(calc, ctx.matchesGolden("tilem-init.png"));

  ctx.pump();

  REQUIRE_THAT(calc, ctx.matchesGolden("tilem-error.png"));

  SECTION("Exit") {
    ctx.tap(ctx.findByText("Exit"));
    ctx.pump();
    REQUIRE(ctx.shouldStop());
  }

  SECTION("Download") {
    // Skip download test in sandboxed environments (like Nix builds)
    // Test for network access with a quick connection test
    if (std::system("wget --spider -q --timeout=2 https://www.google.com >/dev/null 2>&1") != 0) {
      SKIP("Skipping download test - no network access available");
    }

    ctx.tap(ctx.findByText("Download"));

    // Wait for the download to finish.
    ctx.pump();
    while (!ctx.findByText("Downloading ROM '" + romPath.string() + "' ...")
              .empty()) {
      ctx.pump();
    }
    ctx.pump(std::chrono::milliseconds(500));

    REQUIRE_THAT(calc, ctx.matchesGolden("tilem-start.png"));

    ctx.press(ctx.findByType<Keypad>(), { 0.01f, 0.99f });
    ctx.pump(std::chrono::milliseconds(500));
    ctx.release(ctx.findByType<Keypad>(), { 0.01f, 0.99f });

    ctx.pump(std::chrono::milliseconds(500));
    REQUIRE_THAT(calc, ctx.matchesGolden("tilem-on.png"));

    ctx.tap(ctx.findByText("X"));
    ctx.pump();
    REQUIRE(ctx.shouldStop());
  }
}
