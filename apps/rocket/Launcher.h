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
  constexpr static auto title_text = "";
  constexpr static auto sleep_text = "󰒲";

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

  // Also drives the hourly clock icon redraw, piggybacking on the same wake.
  constexpr static auto wakeup_interval = std::chrono::hours(1);
  constexpr static auto battery_warning_percentage = 10;
  constexpr static auto battery_shutdown_percentage = 9;

  constexpr static rmlib::Size splash_size = { 512, 512 };
  constexpr static auto show_timeout = std::chrono::seconds(10);
  // Fallback only - the splash is normally cleared as soon as the launched
  // app takes over the screen (SIGUSR1) or its process exits first.
  constexpr static auto splash_safety_timeout = std::chrono::seconds(15);

public:
  void init(rmlib::AppContext& context, const rmlib::BuildContext& /*unused*/);

  std::string sleepText() const {
    return std::visit(
      overloaded{
        [](const Idle&) -> std::string { return LauncherWidget::title_text; },
        [](const AboutToSuspend&) -> std::string {
          return LauncherWidget::sleep_text;
        },
        [](const CountingDown& cd) -> std::string {
          if (cd.isRetry) {
            return "󰒳";
          }
          const auto progress = std::string(cd.secondsLeft, ' ');
          return "[" + progress + "󰒲" + progress + "]";
        },
      },
      sleepPhase);
  }

  // Tapping the status bar cancels an in-progress sleep countdown, keeping
  // the launcher visible.
  auto statusBar() const {
    using namespace rmlib;

    return GestureDetector(Row(Padding(Text(clockText()), Insets::all(10)),
                               Expanded(Text(sleepText())),
                               Padding(Text(batteryText()), Insets::all(10))),
                           Gestures{}.onTap([this] {
                             if (std::holds_alternative<Idle>(sleepPhase)) {
                               setState([&](auto& self) {
                                 auto* vis =
                                   std::get_if<Visible>(&self.visibility);
                                 if (vis == nullptr) {
                                   return;
                                 }
                                 vis->showMenu = !vis->showMenu;
                               });
                             } else {
                               setState([&](auto& self) { self.stopTimer(); });
                             }
                           }));
  }

  std::string clockText() const {
    auto now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);

    char buf[6];
    std::strftime(buf, sizeof(buf), "%H:%M", &tm);

    constexpr std::array values = {
      "󱑊", "󱐿", "󱑀", "󱑁", "󱑂", "󱑃",
      "󱑄", "󱑅", "󱑆", "󱑇", "󱑈", "󱑉",
    };

    const auto prefix = std::string(values[tm.tm_hour % 12]) + " ";

    if (getWidget().timeOverride != "") {
      return getWidget().timeOverride;
    }

    if (!isMenuOpen()) {
      return prefix;
    }

    return prefix + buf;
  }

  std::string batteryText() const {
    auto battery = getWidget().power.getBattery();
    if (!battery.has_value()) {
      return isMenuOpen() ? "unk% 󱉝 " : "󱉝 ";
    }

    constexpr std::array values = {
      "󰂎", "󰁺", "󰁻", "󰁼", "󰁽", "󰁾",
      "󰁿", "󰂀", "󰂁", "󰂂", "󰁹",
    };

    std::string prefix;
    if (battery->isCharging) {
      prefix = "󰂄 ";
    } else if (battery->percentage < battery_warning_percentage) {
      prefix = "󰂃 ";
    } else {
      const auto scaledPercentage = (battery->percentage - 10) * 100 / 90;
      prefix = std::string(values[scaledPercentage / 10]) + " ";
    }

    if (!isMenuOpen()) {
      return prefix;
    }

    const auto percentageText = std::to_string(battery->percentage);
    const auto pad = std::string(3 - percentageText.size(), ' ');
    return pad + percentageText + "% " + prefix;
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

  auto menu(rmlib::AppContext& ctx) const {
    using namespace rmlib;
    return Column(
      statusBar(),
      Row(
        // TODO: uptime, date, ...
        Expanded(Text("")),
        Sized(
          Column(
            Button("Sleep",
                   [this, &ctx] {
                     setState([&ctx](auto& self) {
                       if (auto* vis = std::get_if<Visible>(&self.visibility);
                           vis != nullptr) {
                         vis->showMenu = false;
                       }
                       self.stopTimer();
                       self.sleepNow(ctx);
                     });
                   }),
            Button("Power Off", [this] { getWidget().power.powerOff(); }),
            Button("Reboot NixOS", [this] { getWidget().power.softReboot(); }),
            Button("Reboot Xochitl", [this] { getWidget().power.reboot(); })),
          500,
          {}),
        // TODO: cpu load, battery consumption insight.
        Expanded(Text(""))),
      // TODO: better 'spacer'
      Expanded(Text("")));
  }

  auto launcher(rmlib::AppContext& context) const {
    using namespace rmlib;
    return Cleared(
      Column(statusBar(), Expanded(Column(runningApps(), appList(context)))));
  }

  auto build(rmlib::AppContext& context,
             const rmlib::BuildContext& /*unused*/) const {
    using namespace rmlib;

    std::vector<DynamicWidget> widgets;
    widgets.emplace_back(launcher(context));

    if (isMenuOpen()) {
      widgets.emplace_back(menu(context));
    }

    if (const auto* splash = std::get_if<Splash>(&splashPhase)) {
      widgets.emplace_back(Center(
        Sized(Image(splash->icon), splash_size.width, splash_size.height)));
    }

    auto ui = Rotated(rotation, Stack(std::move(widgets)));

    // AboutToSuspend must land as a genuine synced GC16 refresh - Sync
    // blocks in swtcon until the draw completes, which is what lets us know
    // "Sleeping" actually hit the panel before suspend is allowed to proceed.
    const bool aboutToSuspend =
      std::holds_alternative<AboutToSuspend>(sleepPhase);

    const auto visible = std::holds_alternative<Visible>(visibility);
    return GestureDetector(Hideable(std::move(ui), visible, aboutToSuspend),
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
    bool showMenu = false;
  };
  // A user/launcher-triggered show() request - self-heals back to Hidden if
  // the SIGCONT ack never arrives.
  struct PendingVisibleLauncher {
    pid_t returnTo = -1;
    rmlib::TimerHandle timeout;
  };
  // An onPrepareSleep()-triggered show() request - no timeout, since the
  // sleep lock already blocks suspend indefinitely until acked or forced.
  struct PendingVisibleSuspend {
    pid_t returnTo = -1;
  };
  using Visibility = std::
    variant<Hidden, PendingVisibleLauncher, PendingVisibleSuspend, Visible>;

  // Sleep countdown, independent of visibility.
  struct Idle {};
  struct CountingDown {
    int secondsLeft;
    bool isRetry = false;
  };
  struct AboutToSuspend {};
  using SleepPhase = std::variant<Idle, CountingDown, AboutToSuspend>;

  // Splash shown while an app launch is in flight, cleared as soon as
  // either the app takes over the screen (SIGUSR1) or its process exits
  // without ever doing so (crash) - safetyTimeout is just a last resort.
  struct NoSplash {};
  struct Splash {
    pid_t pid;
    rmlib::Canvas icon;
    unistdpp::FD pidFd;
    rmlib::FdHandle pidFdHandle;
    rmlib::TimerHandle safetyTimeout;
  };
  using SplashPhase = std::variant<NoSplash, Splash>;

  bool isMenuOpen() const {
    auto* vis = std::get_if<Visible>(&visibility);
    return vis == nullptr ? false : vis->showMenu;
  }

  /// Power off if battery < battery_shutdown_percentage. Also the periodic
  /// redraw that keeps the hourly clock icon current.
  void refreshBattery(bool clearTimer) const;

  /// Sets sleepPhase to CountingDown{time} and starts calling tick every
  /// second.
  void startTimer(rmlib::AppContext& context, bool isRetry = false);
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
  /// Always sets sleepPhase to AboutToSuspend. If rocket is visible, also
  /// releases the sleep lock; otherwise calls showForSuspend() and leaves
  /// the release to onSignal() once that's acked.
  void onPrepareSleep(rmlib::AppContext& context);
  /// Re-acquires the sleep lock; resumes to the current app or retries.
  void onResume(rmlib::AppContext& context, bool wokenByUser);
  /// Defers releasing the sleep lock until after the next draw, so a
  /// just-set AboutToSuspend phase is confirmed on the panel first.
  void releaseSleepLock(rmlib::AppContext& context);

  /// Sends switchTo(self) to rm2fb, returning the previously-active client
  /// to return to (-1 if none/unknown).
  pid_t requestSwitchToSelf();
  /// If Hidden will request rm2fb to become visible, moving to
  /// PendingVisibleLauncher until rm2fb notifies us.
  void show(rmlib::AppContext& context);
  /// Like show(), but moves to PendingVisibleSuspend (no self-heal timeout
  /// needed - the sleep lock bounds the wait instead).
  void showForSuspend(rmlib::AppContext& context);
  /// If Visible will request rm2fb to switch to the 'returnTo' app.
  /// If no app to return to, will suspend (startTimer) if context is not null.
  void hide(rmlib::AppContext* context);
  /// If Visible will stop the timer, try 'hide()' or suspend.
  /// If Hidden will show and start the timer.
  void toggle(rmlib::AppContext& context);

  void launch(rmlib::AppContext& ctx, App& app);
  void switchApp(pid_t pid);

  /// Clears the splash, if any (app became visible, safety timeout, ...).
  void clearSplash();
  /// Called when a launched app's process exits before ever becoming
  /// visible - treated as a crash, so the splash is cleared right away.
  void onSplashAppExited();

  /// On USR1 will stop the timer and set state to Hidden. On CONT will set
  /// state to Visible, finishing a pending suspend or starting the sleep
  /// timer depending on which kind of PendingVisible was acked.
  void onSignal(rmlib::AppContext& context);

  void readApps();
  void requestClients();

  void updateRotation(rmlib::AppContext& ctx);

  void onKey(rmlib::AppContext& ctx, int keycode, bool down) const;

  std::vector<App> apps;

  std::vector<ControlInterface::Client> fbClients;
  std::unordered_map<pid_t, Buffer> fbBuffers;

  unistdpp::FD signalPipe;
  unistdpp::FD wakeupTimerFd;

  rmlib::FdHandle signalFdHandle;
  rmlib::FdHandle wakeupFdHandle;
  rmlib::FdHandle sleepFdHandle;

  rmlib::TimerHandle sleepTimer;
  rmlib::TimerHandle inactivityTimer;

  rmlib::Rotation rotation = rmlib::Rotation::None;

  SleepPhase sleepPhase{ Idle{} };
  SplashPhase splashPhase{ NoSplash{} };
  mutable int inactivityCountdown = default_inactivity_timeout;
  Visibility visibility{ Visible{} };

  bool modPressed = false;
  unistdpp::FD inhibitorLock;
  unistdpp::FD sleepLock;
};
