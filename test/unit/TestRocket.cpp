#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TempFiles.h"
#include "rMLibTestHelper.h"

#include "App.h"
#include "AppWidgets.h"
#include "Launcher.h"

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

TEST_CASE("AppWidget", "[rocket]") {
  auto ctx = TestContext::make();

  App app(AppDescription{ .name = "foo", .command = "/usr/bin/ls" });
  const auto client = ControlInterface::Client{
    .pid = 12,
    .active = true,
    .name = "foo",
  };

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
      client,
      nullptr,
      [&] { tapped++; },
      [&] { killed++; },
      current,
      Rotation::None)));

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

std::vector<AppDescription>
getFakeApps() {
  std::vector<AppDescription> res;
  res.emplace_back(AppDescription{
    .path = "/etc/draft/a.dart",
    .name = "a",
    .description = "A a",
    .command = "sleep 1",
    .icon = "a",
  });
  res.emplace_back(AppDescription{
    .path = "/etc/draft/b.dart",
    .name = "b",
    .command = "yes",
  });

  return res;
}

struct FakeClient : ControlInterface {
  unistdpp::Result<std::vector<Client>> getClients() override { return {}; }
  unistdpp::Result<int> getFramebuffer(pid_t pid) override { return {}; }

  unistdpp::Result<void> switchTo(pid_t pid) override { return {}; }
  unistdpp::Result<void> setLauncher(pid_t pid) override { return {}; }
};

TEST_CASE("Landscape", "[rocket][launcher]") {
  auto client = FakeClient{};

  auto ctx = TestContext::make(/*keyboardAttached=*/true);
  ctx.pumpWidget(Center(LauncherWidget(client, getFakeApps)));
  auto launcher = ctx.findByType<LauncherWidget>();

  REQUIRE_THAT(launcher, ctx.matchesGolden("rocket-landscape.png"));
}

TEST_CASE("Launcher", "[rocket][launcher]") {
  auto client = FakeClient{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, getFakeApps)));
  auto launcher = ctx.findByType<LauncherWidget>();

  REQUIRE_THAT(launcher, ctx.matchesGolden("rocket.png"));
}
