#pragma once

#include "App.h"
#include "AppWidgets.h"
#include "Hideable.h"
#include "Power.h"

#include "rm2fb/SharedBuffer.h"

#include <rm2fb/ControlSocket.h>

#include <UI.h>
#include <UI/Rotate.h>
#include <UI/Stack.h>

#include <ctime>
#include <functional>
#include <utility>
#include <variant>

class LauncherState;

namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
} // namespace

class LauncherWidget : public rmlib::StatefulWidget<LauncherWidget> {
public:
  LauncherWidget(ControlInterface& ctlClient,
                 PowerInterface& power,
                 std::function<std::vector<AppDescription>()> appReader,
                 std::string timeOverride = "")
    : ctlClient(ctlClient)
    , power(power)
    , appReader(std::move(appReader))
    , timeOverride(std::move(timeOverride)) {}

  static LauncherState createState();

  ControlInterface& ctlClient;
  PowerInterface& power;
  std::function<std::vector<AppDescription>()> appReader;
  std::string timeOverride;
};

class LauncherState : public rmlib::StateBase<LauncherWidget> {
  constexpr static auto default_sleep_timeout = 10;
  constexpr static auto retry_sleep_timeout = 8;
  constexpr static auto default_inactivity_timeout = 20;

  constexpr static auto battery_poll_interval = std::chrono::minutes(15);
  constexpr static auto battery_warning_percentage = 10;
  constexpr static auto battery_shutdown_percentage = 9;

  constexpr static rmlib::Size splash_size = { 512, 512 };
  constexpr static auto show_timeout = std::chrono::seconds(10);

public:
  void init(rmlib::AppContext& context, const rmlib::BuildContext& /*unused*/);

  auto header(rmlib::AppContext& context) const {
    using namespace rmlib;

    const auto text = std::visit(
      overloaded{
        [](const Idle&) -> std::string { return "Welcome"; },
        [](const AboutToSuspend&) -> std::string { return "Sleeping"; },
        [](const CountingDown& cd) -> std::string {
          return "Sleeping in : " + std::to_string(cd.secondsLeft);
        },
      },
      sleepPhase);

    auto button = std::visit(
      overloaded{
        [this](const CountingDown&) {
          return Button(
            "Stop", [this] { setState([](auto& self) { self.stopTimer(); }); });
        },
        [](const AboutToSuspend&) {
          // TODO: make hideable?
          return Button("...", [] {});
        },
        [this, &context](const Idle&) {
          return Button("Sleep", [this, &context] {
            setState([&context](auto& self) { self.sleepNow(context); });
          });
        },
      },
      sleepPhase);

    return Center(Padding(
      Column(Padding(Text(text, 2 * default_text_size), Insets::all(10)),
             button),
      Insets::all(50)));
  }

  auto statusBar() const {
    using namespace rmlib;

    return Row(Padding(Text(clockText()), Insets::all(10)),
               Expanded(Text("")),
               Padding(Text(batteryText()), Insets::all(10)));
  }

  std::string clockText() const {
    auto now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);

    char buf[6];
    std::strftime(buf, sizeof(buf), "%H:%M", &tm);
    if (getWidget().timeOverride != "") {
      return getWidget().timeOverride;
    }
    return buf;
  }

  std::string batteryText() const {
    auto battery = getWidget().power.getBattery();
    if (!battery.has_value()) {
      return "󱉝 ";
    }

    std::string prefix = "󰁹 ";
    if (battery->isCharging) {
      prefix = "󰂄 ";
    } else if (battery->percentage < battery_warning_percentage) {
      prefix = "󰂃 ";
    }
    return prefix + std::to_string(battery->percentage) + "%";
  }

  auto runningApps() const {
    using namespace rmlib;

    const auto* vis = std::get_if<Visible>(&visibility);
    const pid_t returnTo = vis != nullptr ? vis->returnTo : -1;

    std::vector<RunningAppWidget> widgets;
    const auto myPid = getpid();
    for (const auto& client : fbClients) {
      if (client.pid == myPid) {
        continue;
      }

      auto fb = fbBuffers.find(client.pid);

      widgets.emplace_back(
        client,
        fb == fbBuffers.end() ? nullptr : &fb->second,
        [this, pid = client.pid] {
          setState([pid](auto& self) { self.switchApp(pid); });
        },
        [this, &client] {
          setState([&client](auto& self) {
            self.stopTimer();
            std::cerr << "stopping " << client.pid << "\n";
            self.switchApp(client.pid);
            kill(-getpgid(client.pid), SIGTERM);
          });
        },
        client.pid == returnTo,
        invert(rotation));
    }
    return Wrap(widgets);
  }

  auto appList(rmlib::AppContext& ctx) const {
    using namespace rmlib;

    std::vector<AppWidget> widgets;
    for (const auto& app : apps) {
      widgets.emplace_back(app, [this, &app, &ctx] {
        setState(
          [&](auto& self) { self.launch(ctx, *const_cast<App*>(&app)); });
      });
    }
    return Wrap(widgets);
  }

  auto launcher(rmlib::AppContext& context) const {
    using namespace rmlib;

    std::vector<DynamicWidget> layers;
    layers.emplace_back(Positioned(statusBar(), Point{ 0, 0 }));
    layers.emplace_back(
      Column(header(context), runningApps(), appList(context)));

    return Cleared(Stack(std::move(layers)));
  }

  auto build(rmlib::AppContext& context,
             const rmlib::BuildContext& /*unused*/) const {
    using namespace rmlib;

    auto ui = [&]() -> std::optional<DynamicWidget> {
      if (!std::holds_alternative<Visible>(visibility)) {
        return {};
      }

      if (background.has_value()) {
        return Center(Rotated(
          rotation,
          Sized(Image(*background), splash_size.width, splash_size.height)));
      }

      return Rotated(rotation, launcher(context));
    }();

    // AboutToSuspend must land as a genuine synced GC16 refresh - FullRefresh
    // blocks in swtcon until the draw completes, which is what lets us know
    // "Sleeping" actually hit the panel before suspend is allowed to proceed.
    const bool aboutToSuspend =
      std::holds_alternative<AboutToSuspend>(sleepPhase);

    return GestureDetector(Hideable(std::move(ui), aboutToSuspend),
                           Gestures{}
                             .onKeyUp([this, &context](auto keyCode) {
                               onKey(context, keyCode, false);
                             })
                             .onKeyDown([this, &context](auto keyCode) {
                               onKey(context, keyCode, true);
                             })
                             .onAny([this]() { resetInactivity(); }));
  }

private:
  // Whether the launcher is the visible/active client, driven externally by
  // the SIGUSR1/SIGCONT the multiplexer sends to ack a switchTo() request.
  struct Hidden {};
  struct Visible {
    pid_t returnTo = -1; // app to switch back to when hidden again.
  };
  struct PendingVisible {
    pid_t returnTo = -1;
    rmlib::TimerHandle timeout;
  };
  using Visibility = std::variant<Hidden, PendingVisible, Visible>;

  // Sleep countdown, independent of visibility.
  struct Idle {};
  struct CountingDown {
    int secondsLeft;
  };
  struct AboutToSuspend {};
  using SleepPhase = std::variant<Idle, CountingDown, AboutToSuspend>;
  /// Power off if battery < battery_shutdown_percentage
  void refreshBattery(bool clearTimer) const;

  /// Sets sleepPhase to CountingDown{time} and starts calling tick every
  /// second.
  void startTimer(rmlib::AppContext& context, int time = default_sleep_timeout);
  /// Cancels an in-progress timer and sets the sleepPhase to Idle.
  void stopTimer();
  /// Decrements the CountingDown timer. Once it reaches zero, requests a
  /// suspend - onPrepareSleep() is what actually moves to AboutToSuspend,
  /// once PrepareForSleep(true) confirms it's really happening.
  void tick(rmlib::AppContext& context) const;
  /// Requests a suspend right away, retrying (via startTimer) if the
  /// request itself fails.
  void sleepNow(rmlib::AppContext& context);

  /// Resets the inactivityCountdown and makes sure an idle lock is taken.
  void resetInactivity() const;
  /// Ticks the inactivity timer. If zero will suspend.
  void tickInactivity(rmlib::AppContext& context) const;

  /// Take an idle inhibitor lock, so logind doesn't behave weird.
  void takeInhibitorLock();
  /// Release the idle inhibitor.
  void releaseInhibitorLock();

  /// Take the sleep delay lock, deferring an in-progress suspend.
  void takeSleepLock();

  /// Handles a PrepareForSleep transition read from pollSleep().
  void onSleepFdReady(rmlib::AppContext& context);
  /// Sets sleepPhase to AboutToSuspend if rocket is visible, which build()
  /// turns into a synced "Sleeping" draw before the sleep lock is released.
  void onPrepareSleep(rmlib::AppContext& context);
  /// Re-acquires the sleep lock; resumes to the current app or retries.
  void onResume(rmlib::AppContext& context, bool wokenByUser);

  /// If Hidden will request rm2fb to become visible.
  /// Will store the current active client in 'returnTo'.
  /// visibility is PendingVisible until rm2fb notifies us.
  void show(rmlib::AppContext& context);
  /// If Visible will request rm2fb to switch to the 'returnTo' app.
  /// If no app to return to, will suspend (startTimer) if context is not null.
  void hide(rmlib::AppContext* context);
  /// If Visible will stop the timer, try 'hide()' or suspend.
  /// If Hidden will show and start the timer.
  void toggle(rmlib::AppContext& context);

  void launch(rmlib::AppContext& ctx, App& app);
  void switchApp(pid_t pid);

  /// On USR1 will stop the timer and set state to Hidden.
  /// On CONT will set state to Visible and start the sleep timer.
  /// Also refreshes apps & clients.
  void onSignal(rmlib::AppContext& context);

  void readApps();
  void requestClients();

  void updateRotation(rmlib::AppContext& ctx);

  void onKey(rmlib::AppContext& ctx, int keycode, bool down) const;

  std::vector<App> apps;

  std::vector<ControlInterface::Client> fbClients;
  std::unordered_map<pid_t, Buffer> fbBuffers;

  unistdpp::FD signalPipe;
  unistdpp::FD batteryTimerFd;

  rmlib::TimerHandle sleepTimer;
  rmlib::TimerHandle inactivityTimer;
  rmlib::TimerHandle backgroundTimer;
  rmlib::TimerHandle clockTimer;

  rmlib::Rotation rotation = rmlib::Rotation::None;
  std::optional<rmlib::Canvas> background;

  SleepPhase sleepPhase{ Idle{} };
  mutable int inactivityCountdown = default_inactivity_timeout;
  Visibility visibility{ Visible{} };

  bool modPressed = false;
  unistdpp::FD inhibitorLock;
  unistdpp::FD sleepLock;
};
