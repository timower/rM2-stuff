#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TempFiles.h"
#include "rMLibTestHelper.h"

#include "App.h"
#include "AppWidgets.h"
#include "Hideable.h"
#include "Launcher.h"

#include <unistdpp/pipe.h>
#include <unistdpp/poll.h>

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

constexpr static auto about_to_sleep_text = "[          󰒲          ]";
constexpr static auto retry_sleep_text = "󰒳";

TEST_CASE("Hideable: forceRefresh requests a genuine GC16 full refresh",
          "[rocket]") {
  auto ctx = TestContext::make();
  const auto constraints = Constraints{ { 0, 0 }, { 100, 100 } };

  Hideable<Text> initial{ Text("hi"), false };
  auto ro = initial.createRenderObject();
  auto& hideableRO = static_cast<HideableRenderObject<Text>&>(*ro);

  hideableRO.layout(constraints);
  {
    std::vector<UpdateRegion> initial;
    hideableRO.draw(
      ctx.getFramebuffer().canvas, { 0, 0 }, initial); // consume initial draw.
  }

  SECTION("becoming visible without forceRefresh only upgrades the waveform") {
    Hideable<Text> shown{ Text("hi"), true };
    hideableRO.update(shown);
    hideableRO.layout(constraints);

    std::vector<UpdateRegion> updates;
    hideableRO.draw(ctx.getFramebuffer().canvas, { 0, 0 }, updates);
    REQUIRE(updates.size() == 1);
    CHECK(updates[0].waveform == fb::Waveform::GC16Fast);
    CHECK(updates[0].flags == fb::UpdateFlags::None);
  }

  SECTION("forceRefresh forces GC16 + Sync") {
    Hideable<Text> shown{ Text("hi"), true, /*forceRefresh=*/true };
    hideableRO.update(shown);
    hideableRO.layout(constraints);

    std::vector<UpdateRegion> updates;
    hideableRO.draw(ctx.getFramebuffer().canvas, { 0, 0 }, updates);
    REQUIRE(updates.size() == 1);
    CHECK(updates[0].waveform == fb::Waveform::GC16Fast);
    CHECK(updates[0].flags == fb::UpdateFlags::Sync);
  }

  SECTION("forceRefresh is consumed after a single draw") {
    Hideable<Text> shown{ Text("hi"), true, /*forceRefresh=*/true };
    hideableRO.update(shown);
    hideableRO.layout(constraints);
    {
      std::vector<UpdateRegion> updates;
      hideableRO.draw(ctx.getFramebuffer().canvas, { 0, 0 }, updates);
    }

    hideableRO.markNeedsDraw();
    std::vector<UpdateRegion> updates;
    hideableRO.draw(ctx.getFramebuffer().canvas, { 0, 0 }, updates);
    REQUIRE(updates.size() == 1);
    CHECK(updates[0].flags == fb::UpdateFlags::None);
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

// A single fake app with a real, loadable icon - unlike getFakeApps()'s
// apps, whose .icon never resolves to an actual file - so launching it
// exercises LauncherState::launch()'s splash path.
std::function<std::vector<AppDescription>()>
getFakeAppWithIcon(std::string command) {
  return [command] {
    std::vector<AppDescription> res;
    res.emplace_back(AppDescription{
      .path = "/etc/draft/icon.dart",
      .name = "icon-app",
      .command = command,
      .iconPath = (assets_path / "rocket.png").string(),
    });
    return res;
  };
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

// Simulates the real fd+pollSleep() surface SystemPowerInterface exposes,
// so tests exercise the same context.listenFd()/pollSleep() wiring
// LauncherState actually uses, rather than bypassing it. Lock fds are real
// pipe ends too, so a test can observe whether Launcher actually released
// one (see sleepLockReleased()) instead of trusting a self-reported flag.
struct FakePower : PowerInterface {
  std::optional<rmlib::device::BatteryInfo> battery =
    rmlib::device::BatteryInfo{ .percentage = 100, .isCharging = false };
  bool didPowerOff = false;
  bool didReboot = false;
  bool didSoftReboot = false;

  int suspendRequests = 0;
  bool suspendShouldFail = false;

  int acquireSleepLockCalls = 0;
  unistdpp::FD sleepLockReadEnd; // test-visible: see sleepLockReleased().

  int acquireIdleLockCalls = 0;

  std::optional<SleepUpdate> pendingUpdate;
  unistdpp::FD sleepReadFd;
  unistdpp::FD sleepWriteFd;

  FakePower() {
    auto pipe = unistdpp::fatalOnError(unistdpp::pipe());
    sleepReadFd = std::move(pipe.readPipe);
    sleepWriteFd = std::move(pipe.writePipe);
  }

  std::optional<rmlib::device::BatteryInfo> getBattery() override {
    return battery;
  }
  void powerOff() override { didPowerOff = true; }
  void reboot() override { didReboot = true; }
  void softReboot() override { didSoftReboot = true; }

  int sleepFd() override { return sleepReadFd.fd; }

  std::optional<SleepUpdate> pollSleep() override {
    // Non-blocking like the real bus fd: onSleepFdReady() calls this in a
    // loop until it reports nothing left, so a second call with no pending
    // byte must not block waiting for one.
    std::vector<pollfd> fds{ unistdpp::waitFor(sleepReadFd,
                                               unistdpp::Wait::Read) };
    auto res = unistdpp::poll(fds, std::chrono::milliseconds(0));
    if (!res.has_value() || *res == 0) {
      return std::nullopt;
    }

    sleepReadFd.readAll<char>().or_else([](auto /*unused*/) {});
    return std::exchange(pendingUpdate, std::nullopt);
  }

  bool requestSuspend() override {
    suspendRequests++;
    return !suspendShouldFail;
  }

  unistdpp::Result<unistdpp::FD> acquireSleepLock() override {
    acquireSleepLockCalls++;
    auto pipe = unistdpp::fatalOnError(unistdpp::pipe());
    sleepLockReadEnd = std::move(pipe.readPipe);
    return std::move(pipe.writePipe);
  }

  unistdpp::Result<unistdpp::FD> acquireIdleLock() override {
    acquireIdleLockCalls++;
    auto pipe = unistdpp::fatalOnError(unistdpp::pipe());
    return std::move(pipe.writePipe);
  }
};

// True once the write end handed out by the most recent acquireSleepLock()
// call has been closed - i.e. the caller actually released the lock,
// observed the same way a real pipe/eventfd consumer would (EOF/HUP).
bool
sleepLockReleased(FakePower& power) {
  std::vector<pollfd> fds{ unistdpp::waitFor(power.sleepLockReadEnd,
                                             unistdpp::Wait::Read) };
  auto res = unistdpp::poll(fds, std::chrono::milliseconds(0));
  return res.has_value() && *res > 0 && unistdpp::isClosed(fds[0]);
}

// Makes power's sleepFd() readable with `update` pending, as
// SystemPowerInterface's real fd would once a PrepareForSleep signal
// arrives, then pumps the context so it gets processed.
void
triggerSleepEvent(TestContext& ctx,
                  FakePower& power,
                  PowerInterface::SleepUpdate update) {
  power.pendingUpdate = update;
  power.sleepWriteFd.writeAll('x').or_else([](auto /*unused*/) {});
  ctx.pump(std::chrono::milliseconds(10));
}

// Simulates logind's PrepareForSleep(false) resume signal.
void
simulateResume(TestContext& ctx, FakePower& power, bool wokenByUser) {
  triggerSleepEvent(
    ctx, power, { .sleeping = false, .wokenByUser = wokenByUser });
}

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

TEST_CASE("Launcher FSM: toggle shows the countdown immediately but only "
          "starts ticking once visible",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  client.clients = { ControlInterface::Client{
    .pid = 4242, .active = true, .format = {}, .name = "other" } };
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  // Starts visible (rocket is the foreground app when it launches).
  REQUIRE_FALSE(ctx.findByText(LauncherWidget::title_text).empty());

  // The multiplexer switches away: launcher goes hidden. (Hideable doesn't
  // detach the render tree when hiding, only skips drawing/input for it, so
  // we can't assert "Welcome" is gone here - only that becoming visible
  // again behaves as if it were. See show()'s Hidden guard below.)
  raiseAndPump(ctx, SIGUSR1);

  // User presses power: the countdown shows right away, but stays frozen -
  // it must not actually tick down until the switch is confirmed.
  pressPower(ctx);
  CHECK(client.switchToCount == 1);
  CHECK(client.lastSwitchTo == getpid());
  REQUIRE_FALSE(ctx.findByText(about_to_sleep_text).empty());

  ctx.pump(std::chrono::milliseconds(1100));
  REQUIRE_FALSE(ctx.findByText(about_to_sleep_text).empty());

  // The multiplexer acks the switch: *now* it starts ticking down.
  raiseAndPump(ctx, SIGCONT);
  ctx.pump(std::chrono::milliseconds(1100));
  REQUIRE(ctx.findByText(about_to_sleep_text).empty());
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
  // launcher reverts to hidden and the frozen countdown clears too, so a
  // fresh press issues a new request instead of showing stale state.
  ctx.pump(std::chrono::milliseconds(10500));
  REQUIRE(ctx.findByText(about_to_sleep_text).empty());
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
  REQUIRE_FALSE(ctx.findByText(about_to_sleep_text).empty());

  // Cancel the countdown via a tap on the status bar.
  auto countdown = ctx.findByText(about_to_sleep_text);
  REQUIRE(countdown.size() == 1);
  ctx.tap(countdown);
  ctx.pump();
  REQUIRE_FALSE(ctx.findByText(LauncherWidget::title_text).empty());

  // Pressing power again returns to the known app rather than sleeping.
  pressPower(ctx);
  CHECK(client.lastSwitchTo == 777);
}

TEST_CASE(
  "Launcher FSM: pressing power sleeps immediately when nothing to return to",
  "[rocket][launcher]") {
  auto client = FakeClient{}; // getClients() -> empty, no known active app.
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  // Launcher starts visible & idle; with nothing to return to, pressing
  // power should attempt to sleep right away rather than no-op.
  pressPower(ctx);
  ctx.pump(std::chrono::milliseconds(50));

  CHECK(power.suspendRequests == 1);
  CHECK(client.switchToCount == 0);

  // logind confirms it's actually suspending.
  triggerSleepEvent(ctx, power, { .sleeping = true, .wokenByUser = false });

  // Something other than the user woke it back up - retry.
  simulateResume(ctx, power, /*wokenByUser=*/false);
  REQUIRE_FALSE(ctx.findByText(retry_sleep_text).empty()); // retry timeout
}

TEST_CASE("Launcher FSM: a successful sleep returns to idle",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  pressPower(ctx); // Idle -> requests suspend immediately (no returnTo).
  ctx.pump(std::chrono::milliseconds(50));
  CHECK(power.suspendRequests == 1);

  // logind confirms it's actually suspending.
  triggerSleepEvent(ctx, power, { .sleeping = true, .wokenByUser = false });

  simulateResume(ctx, power, /*wokenByUser=*/true);

  REQUIRE_FALSE(ctx.findByText(LauncherWidget::title_text).empty());
  CHECK(client.switchToCount == 0); // Nothing to switch back to.
}

TEST_CASE(
  "Launcher FSM: a failed suspend request retries without waiting for a "
  "resume signal",
  "[rocket][launcher]") {
  // Distinct from the "pressing power sleeps immediately..." case above:
  // there the *request* succeeds and it's the resume reason that's not the
  // user; here the request itself fails, so no PrepareForSleep(false) will
  // ever arrive to drive the retry - tick() must notice synchronously.
  auto client = FakeClient{};
  auto power = FakePower{};
  power.suspendShouldFail = true;

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  pressPower(ctx); // Idle -> requests suspend immediately (no returnTo).
  ctx.pump(std::chrono::milliseconds(50));

  CHECK(power.suspendRequests == 1);
  REQUIRE_FALSE(ctx.findByText(retry_sleep_text).empty()); // retry timeout
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
  REQUIRE_FALSE(ctx.findByText(about_to_sleep_text).empty());

  ctx.pump(std::chrono::milliseconds(1100));
  REQUIRE_FALSE(ctx
                  .findByText("[         " +
                              std::string(LauncherWidget::sleep_text) +
                              "         ]")
                  .empty());
}

TEST_CASE("Launcher FSM: an externally-triggered suspend forces a redraw "
          "before releasing the sleep lock",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  REQUIRE(power.acquireSleepLockCalls == 1); // acquired on startup.
  CHECK_FALSE(sleepLockReleased(power));
  REQUIRE_FALSE(ctx.findByText(LauncherWidget::title_text).empty());

  triggerSleepEvent(ctx, power, { .sleeping = true, .wokenByUser = false });

  CHECK(sleepLockReleased(power));
  REQUIRE_FALSE(ctx.findByText(LauncherWidget::sleep_text).empty());
}

TEST_CASE("Launcher FSM: an external suspend while hidden requests to "
          "become visible before releasing the sleep lock",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  client.clients = { ControlInterface::Client{
    .pid = 4242, .active = true, .format = {}, .name = "other" } };
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  // The multiplexer switches away: another app is on screen.
  raiseAndPump(ctx, SIGUSR1);
  REQUIRE(power.acquireSleepLockCalls == 1); // acquired on startup.

  // logind wants to suspend while rocket isn't the visible client.
  triggerSleepEvent(ctx, power, { .sleeping = true, .wokenByUser = false });

  // The sleep screen can't reach the panel until rocket is visible again -
  // it must request that instead of releasing the lock right away.
  CHECK(client.switchToCount == 1);
  CHECK(client.lastSwitchTo == getpid());
  CHECK_FALSE(sleepLockReleased(power));

  // Once the multiplexer acks the switch, the lock is finally released.
  raiseAndPump(ctx, SIGCONT);
  CHECK(sleepLockReleased(power));
  REQUIRE_FALSE(ctx.findByText(LauncherWidget::sleep_text).empty());
}

TEST_CASE("Launcher FSM: a resume that beats the show() ack clears "
          "sleepPhase instead of leaving it stuck on AboutToSuspend",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  client.clients = { ControlInterface::Client{
    .pid = 4242, .active = true, .format = {}, .name = "other" } };
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(Center(LauncherWidget(client, power, getFakeApps)));

  raiseAndPump(ctx, SIGUSR1);
  triggerSleepEvent(ctx, power, { .sleeping = true, .wokenByUser = false });
  CHECK(client.switchToCount == 1); // requested to become visible.

  // logind forces the suspend through before the multiplexer ever acks the
  // switch - resume arrives with no SIGCONT ever having landed.
  simulateResume(ctx, power, /*wokenByUser=*/true);

  // sleepPhase must not be left stuck on AboutToSuspend.
  REQUIRE_FALSE(ctx.findByText(LauncherWidget::title_text).empty());
  REQUIRE(ctx.findByText(LauncherWidget::sleep_text).empty());
}

TEST_CASE("Launcher FSM: the launch splash clears once the app becomes "
          "visible via SIGUSR1",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make();
  // Short-lived on purpose: a real, not-yet-reaped child inherits this
  // test binary's stdout/stderr, and ctest's output capture only sees EOF
  // once every process holding those fds has exited - a long-lived child
  // here would stall the test run for its whole lifetime.
  ctx.pumpWidget(
    Center(LauncherWidget(client, power, getFakeAppWithIcon("sleep 0.2"))));

  auto appWidget = ctx.findByType<AppWidget>();
  REQUIRE(appWidget.size() == 1);
  const auto imagesBeforeSplash = ctx.findByType<Image>().size();

  ctx.tap(appWidget);
  ctx.pump();

  // Splash is up, overlaid on top of (not replacing) the normal launcher UI.
  CHECK(ctx.findByType<Image>().size() == imagesBeforeSplash + 1);

  // The multiplexer acks the app taking over the screen, then rocket is
  // shown again later.
  raiseAndPump(ctx, SIGUSR1);
  pressPower(ctx);
  raiseAndPump(ctx, SIGCONT);

  // Had SIGUSR1 not cleared the splash, its extra image would still be up.
  CHECK(ctx.findByType<Image>().size() == imagesBeforeSplash);
}

TEST_CASE("Launcher FSM: the launch splash clears if the app exits before "
          "becoming visible",
          "[rocket][launcher]") {
  auto client = FakeClient{};
  auto power = FakePower{};

  auto ctx = TestContext::make();
  ctx.pumpWidget(
    Center(LauncherWidget(client, power, getFakeAppWithIcon("true"))));

  auto appWidget = ctx.findByType<AppWidget>();
  REQUIRE(appWidget.size() == 1);
  const auto imagesBeforeSplash = ctx.findByType<Image>().size();

  ctx.tap(appWidget);
  ctx.pump();

  // Splash is up.
  CHECK(ctx.findByType<Image>().size() == imagesBeforeSplash + 1);

  // "true" exits almost instantly without ever touching rm2fb - no SIGUSR1
  // ever arrives. The pidfd should catch that and clear the splash well
  // before the 15s safety timeout.
  ctx.pump(std::chrono::milliseconds(200));
  CHECK(ctx.findByType<Image>().size() == imagesBeforeSplash);
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

  // Percentage is only shown once the menu is open; the icon alone is
  // enough to confirm the warning threshold was picked up.
  REQUIRE_FALSE(ctx.findByText("\U000f0083 ").empty());
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

  // Percentage is only shown once the menu is open; the icon alone is
  // enough to confirm charging suppresses the warning icon.
  REQUIRE_FALSE(ctx.findByText("\U000f0084 ").empty());
  CHECK_FALSE(power.didPowerOff);
}
