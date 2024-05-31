#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TempFiles.h"
#include "rMLibTestHelper.h"

#include "App.h"
#include "AppWidgets.h"
#include "Launcher.h"

#include "unistdpp/file.h"

#include <fstream>

using namespace rmlib;

void
writeFile(const std::filesystem::path& path, std::string_view txt) {
  std::ofstream ofs(path.string());
  REQUIRE(ofs.is_open());

  ofs << txt;
}

TEST_CASE("App::read", "[rocket]") {
  TemporaryDirectory tmp;

  SECTION("Basic reading") {
    const auto path = tmp.dir / "basic.draft";
    writeFile(path, R"(
name=xochitl
desc=Read documents and take notes
call=/usr/bin/xochitl
term=:
imgFile=xochitl
    )");

    auto app = AppDescription::read(path.c_str(), (tmp.dir / "icons").c_str());
    REQUIRE(app.has_value());

    CHECK(app->name == "xochitl");
    CHECK(app->description == "Read documents and take notes");
    CHECK(app->command == "/usr/bin/xochitl");
    CHECK(app->icon == "xochitl");
    CHECK(app->iconPath == tmp.dir / "icons" / "xochitl.png");
  }

  SECTION("No command") {
    const auto path = tmp.dir / "basic.draft";
    writeFile(path, R"(
name=xochitl
desc=Read documents and take notes
term=:
imgFile=xochitl
    )");

    auto app = AppDescription::read(path.c_str(), (tmp.dir / "icons").c_str());
    REQUIRE_FALSE(app.has_value());
  }

  SECTION("No name") {
    const auto path = tmp.dir / "basic.draft";
    writeFile(path, R"(
desc=Read documents and take notes
call=/usr/bin/xochitl
term=:
imgFile=xochitl
    )");

    auto app = AppDescription::read(path.c_str(), (tmp.dir / "icons").c_str());
    REQUIRE_FALSE(app.has_value());
  }

  SECTION("Only name and command") {
    const auto path = tmp.dir / "basic.draft";
    writeFile(path, R"(
name=xochitl
call=/usr/bin/xochitl
    )");

    auto app = AppDescription::read(path.c_str(), (tmp.dir / "icons").c_str());
    REQUIRE(app.has_value());
    CHECK(app->name == "xochitl");
    CHECK(app->command == "/usr/bin/xochitl");
  }
}

TEST_CASE("readAppFiles", "[rocket]") {
  TemporaryDirectory tmp;

  writeFile(tmp.dir / "a.draft", R"(
name=a
desc=A a
call=/usr/bin/a
term=:
imgFile=a
    )");
  writeFile(tmp.dir / "b.draft", R"(
name=b
call=/usr/bin/b
    )");
  writeFile(tmp.dir / "c", R"(
name=c
call=/usr/bin/c --foo
imgFile=c
    )");

  auto files = readAppFiles(tmp.dir.c_str());

  REQUIRE(files.size() == 3);

  const auto& a = files[0];
  CHECK(a.name == "a");
  CHECK(a.description == "A a");
  CHECK(a.command == "/usr/bin/a");
  CHECK(a.icon == "a");
  CHECK(a.iconPath == tmp.dir / "icons" / "a.png");

  const auto& b = files[1];
  CHECK(b.name == "b");
  CHECK(b.command == "/usr/bin/b");

  const auto& c = files[2];
  CHECK(c.name == "c");
  CHECK(c.command == "/usr/bin/c --foo");
  CHECK(c.iconPath == tmp.dir / "icons" / "c.png");
}

TEST_CASE("App", "[rocket]") {
  App app(AppDescription{ .name = "yes", .command = "sleep 5000" });

  REQUIRE(app.launch());
  usleep(500);

  REQUIRE(app.isRunning());
  REQUIRE_FALSE(app.isPaused());

  app.stop();

  while (!AppManager::getInstance().update()) {
    usleep(500);
  }

  REQUIRE_FALSE(app.isRunning());
}

TEST_CASE("AppWidget", "[rocket]") {
  auto ctx = TestContext::make();

  App app(AppDescription{ .name = "foo", .command = "/usr/bin/ls" });

  SECTION("AppWidget") {
    int clicked = 0;
    ctx.pumpWidget(Center(AppWidget(app, [&] { clicked++; })));

    auto appWidget = ctx.findByType<AppWidget>();
    REQUIRE_THAT(appWidget, ctx.matchesGolden("missing-icon-app.png"));

    ctx.tap(appWidget);
    REQUIRE(clicked == 1);
  }

  SECTION("RunningAppWidget") {
    int tapped = 0;
    int killed = 0;

    bool current = GENERATE(true, false);

    ctx.pumpWidget(Center(RunningAppWidget(
      app, [&] { tapped++; }, [&] { killed++; }, current)));

    auto appWidget = ctx.findByType<RunningAppWidget>();
    REQUIRE_THAT(
      appWidget,
      ctx.matchesGolden("running-current-" + std::to_string(current) + ".png"));

    ctx.tap(appWidget);
    REQUIRE(tapped == 1);

    auto closeTxt = ctx.findByText("X");
    ctx.tap(closeTxt);
    REQUIRE(killed == 1);
  }
}

TEST_CASE("Launcher", "[rocket]") {
  TemporaryDirectory tmp;

  const auto configPath = tmp.dir / ".config" / "draft";
  REQUIRE_NOTHROW(std::filesystem::create_directories(configPath));
  setenv("HOME", tmp.dir.c_str(), true);

  writeFile(configPath / "a.draft", R"(
name=a
desc=A a
call=sleep 1
term=:
imgFile=a
    )");
  writeFile(configPath / "b.draft", R"(
name=b
call=yes
    )");

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget()));
  auto launcher = ctx.findByType<LauncherWidget>();

  REQUIRE_THAT(launcher, ctx.matchesGolden("rocket.png"));

  auto labelA = ctx.findByText("a");
  ctx.tap(labelA);
  ctx.pump();

  REQUIRE_THAT(launcher, ctx.matchesGolden("rocket_a.png"));

  sleep(2);
  ctx.pump();

  REQUIRE_THAT(launcher, ctx.matchesGolden("rocket.png"));
}
