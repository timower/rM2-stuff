#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TempFiles.h"
#include "rMLibTestHelper.h"

#include "App.h"
#include "AppWidgets.h"
#include "Launcher.h"

#include <csignal>
#include <fstream>
#include <unistd.h>

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
    .format = {},
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
  std::vector<Client> clients;
  pid_t lastSwitchTo = -1;
  int switchToCount = 0;

  unistdpp::Result<std::vector<Client>> getClients() override {
    return clients;
  }
  unistdpp::Result<int> getFramebuffer(pid_t pid) override { return {}; }

  unistdpp::Result<void> switchTo(pid_t pid) override {
    lastSwitchTo = pid;
    switchToCount++;
    return {};
  }
  unistdpp::Result<void> setLauncher(pid_t pid) override { return {}; }
};

struct FakePower : PowerInterface {
  std::optional<rmlib::device::BatteryInfo> battery =
    rmlib::device::BatteryInfo{ .percentage = 100, .isCharging = false };
  bool suspendResult = true;
  bool didPowerOff = false;

  std::optional<rmlib::device::BatteryInfo> getBattery() override {
    return battery;
  }
  bool suspend() override { return suspendResult; }
  void powerOff() override { didPowerOff = true; }
};

// Simulates a physical key press/release, e.g. the power button.
void
sendKey(TestContext& ctx, int keyCode, bool down) {
  SDL_Event ev{};
  ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  ev.key.keysym.scancode = static_cast<SDL_Scancode>(keyCode);
  ctx.sendEv(ev);
}

void
pressPower(TestContext& ctx) {
  // Under EMULATE, Launcher.cpp's KEY_POWER is remapped to SDL_SCANCODE_POWER
  // (see EmulatedKeyCodes.h) rather than the real evdev KEY_POWER (116).
  sendKey(ctx, SDL_SCANCODE_POWER, true);
  sendKey(ctx, SDL_SCANCODE_POWER, false);
  ctx.pump(std::chrono::milliseconds(10));
}

// Simulates the multiplexer acking/revoking a switch via SIGCONT/SIGUSR1.
void
raiseAndPump(TestContext& ctx, int sig) {
  raise(sig);
  ctx.pump(std::chrono::milliseconds(20));
}

TEST_CASE("Landscape", "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make(/*keyboardAttached=*/true);
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps, "13:37")));
  auto launcher = ctx.findByType<LauncherWidget>();

  REQUIRE_THAT(launcher, ctx.matchesGolden("rocket-landscape.png"));
}

TEST_CASE("Launcher", "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make();

  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps, "13:37")));
  auto launcher = ctx.findByType<LauncherWidget>();

  REQUIRE_THAT(launcher, ctx.matchesGolden("rocket.png"));
}

TEST_CASE("Launcher FSM: toggle defers the sleep timer until visible",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  client.clients = { ControlInterface::Client{
    .pid = 4242, .active = true, .format = {}, .name = "other" } };
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  // Starts visible (rocket is the foreground app when it launches).
  REQUIRE_FALSE(ctx.findByText("Welcome").empty());

  // The multiplexer switches away: launcher goes hidden. (Hideable doesn't
  // detach the render tree when hiding, only skips drawing/input for it, so
  // we can't assert "Welcome" is gone here - only that becoming visible
  // again behaves as if it were. See show()'s Hidden guard below.)
  raiseAndPump(ctx, SIGUSR1);

  // User presses power: requests to become visible again, but the sleep
  // countdown must not start until that's confirmed.
  pressPower(ctx);
  CHECK(client.switchToCount == 1);
  CHECK(client.lastSwitchTo == getpid());
  REQUIRE(ctx.findByText("Sleeping in : 10").empty());

  // The multiplexer acks the switch: *now* the countdown starts.
  raiseAndPump(ctx, SIGCONT);
  REQUIRE_FALSE(ctx.findByText("Sleeping in : 10").empty());
}

TEST_CASE("Launcher FSM: a pending show self-heals if the ack never arrives",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  raiseAndPump(ctx, SIGUSR1);

  pressPower(ctx);
  CHECK(client.switchToCount == 1);

  // A repeated press while a show request is already pending is ignored.
  pressPower(ctx);
  CHECK(client.switchToCount == 1);

  // Once the pending request times out (no SIGCONT ever arrived), the
  // launcher reverts to hidden, so a fresh press issues a new request.
  ctx.pump(std::chrono::milliseconds(10500));
  pressPower(ctx);
  CHECK(client.switchToCount == 2);
}

TEST_CASE(
  "Launcher FSM: hide() returns to the previous app, or sleeps if none is "
  "known",
  "[rocket][launcher]") {
  auto client = FakeClient{};
  client.clients = { ControlInterface::Client{
    .pid = 777, .active = true, .format = {}, .name = "other" } };
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  raiseAndPump(ctx, SIGUSR1);
  pressPower(ctx);
  raiseAndPump(ctx, SIGCONT); // Visible{returnTo=777}, counting down.
  REQUIRE_FALSE(ctx.findByText("Sleeping in : 10").empty());

  // Cancel the countdown via the Stop button.
  auto stop = ctx.findByText("Stop");
  REQUIRE(stop.size() == 1);
  ctx.tap(stop);
  ctx.pump();
  REQUIRE_FALSE(ctx.findByText("Welcome").empty());

  // Pressing power again returns to the known app rather than sleeping.
  pressPower(ctx);
  CHECK(client.lastSwitchTo == 777);
}

TEST_CASE(
  "Launcher FSM: pressing power sleeps immediately when nothing to return to",
  "[rocket][launcher]") {
  // Force the retry branch so it's observable instead of racing a real tick.
  auto client = FakeClient{}; // getClients() -> empty, no known active app.
  auto power = FakePower{};
  power.suspendResult = false;

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  // Launcher starts visible & idle; with nothing to return to, pressing
  // power should attempt to sleep right away rather than no-op.
  pressPower(ctx);
  ctx.pump(std::chrono::milliseconds(50));

  REQUIRE_FALSE(ctx.findByText("Sleeping in : 8").empty()); // retry timeout
  CHECK(client.switchToCount == 0);
}

TEST_CASE("Launcher FSM: a successful sleep returns to idle",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  pressPower(ctx); // Idle -> AboutToSuspend (immediate, no returnTo).
  ctx.pump(std::chrono::milliseconds(50));

  REQUIRE_FALSE(ctx.findByText("Welcome").empty());
  CHECK(client.switchToCount == 0); // Nothing to switch back to.
}

TEST_CASE("Launcher FSM: sleep countdown decrements every second",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  client.clients = { ControlInterface::Client{
    .pid = 555, .active = true, .format = {}, .name = "other" } };
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  raiseAndPump(ctx, SIGUSR1);
  pressPower(ctx);
  raiseAndPump(ctx, SIGCONT);
  REQUIRE_FALSE(ctx.findByText("Sleeping in : 10").empty());

  ctx.pump(std::chrono::milliseconds(1100));
  REQUIRE_FALSE(ctx.findByText("Sleeping in : 9").empty());
}

TEST_CASE("Launcher battery: shows a warning icon below 10%, unrelated to "
          "the shutdown threshold",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};
  power.battery =
    rmlib::device::BatteryInfo{ .percentage = 9, .isCharging = false };

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps, "13:37")));

  REQUIRE_FALSE(ctx.findByText("\U000f0083 9%").empty());
  CHECK_FALSE(power.didPowerOff);
}

TEST_CASE("Launcher battery: requests a shutdown below 9%",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};
  power.battery =
    rmlib::device::BatteryInfo{ .percentage = 8, .isCharging = false };

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps, "13:37")));

  CHECK(power.didPowerOff);
}

TEST_CASE("Launcher battery: charging suppresses the shutdown even below 9%",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};
  power.battery =
    rmlib::device::BatteryInfo{ .percentage = 5, .isCharging = true };

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps, "13:37")));

  REQUIRE_FALSE(ctx.findByText("\U000f0084 5%").empty());
  CHECK_FALSE(power.didPowerOff);
}
