#pragma once

#include "App.h"
#include "AppWidgets.h"
#include "Hideable.h"

#include "rm2fb/SharedBuffer.h"

#include <rm2fb/ControlSocket.h>

#include <UI.h>
#include <UI/Rotate.h>

#include <utility>

class LauncherState;

class LauncherWidget : public rmlib::StatefulWidget<LauncherWidget> {
public:
  LauncherWidget(ControlInterface& ctlClient,
                 std::function<std::vector<AppDescription>()> appReader)
    : ctlClient(ctlClient), appReader(std::move(appReader)) {}
  static LauncherState createState();

  ControlInterface& ctlClient;
  std::function<std::vector<AppDescription>()> appReader;
};

class LauncherState : public rmlib::StateBase<LauncherWidget> {
  constexpr static auto default_sleep_timeout = 10;
  constexpr static auto retry_sleep_timeout = 8;
  constexpr static auto default_inactivity_timeout = 20;

  constexpr static rmlib::Size splash_size = { 512, 512 };

public:
  void init(rmlib::AppContext& context, const rmlib::BuildContext& /*unused*/);

  auto header(rmlib::AppContext& context) const {
    using namespace rmlib;

    const auto text = [this]() -> std::string {
      switch (sleepCountdown) {
        case -1:
          return "Welcome";
        case 0:
          return "Sleeping";
        default:
          return "Sleeping in : " + std::to_string(sleepCountdown);
      }
    }();

    auto button = [this, &context] {
      if (sleepCountdown > 0) {
        return Button(
          "Stop", [this] { setState([](auto& self) { self.stopTimer(); }); });
      }
      if (sleepCountdown == 0) {
        // TODO: make hideable?
        return Button("...", [] {});
      }
      return Button("Sleep", [this, &context] {
        setState([&context](auto& self) { self.startTimer(context, 0); });
      });
    }();

    return Center(Padding(
      Column(Padding(Text(text, 2 * default_text_size), Insets::all(10)),
             button),
      Insets::all(50)));
  }

  auto runningApps() const {
    using namespace rmlib;

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
        client.active,
        invert(rotation));
    }
    return Wrap(widgets);
  }

  auto appList() const {
    using namespace rmlib;

    std::vector<AppWidget> widgets;
    for (const auto& app : apps) {
      if (!isRunning(app.getLaunchPid())) {
        widgets.emplace_back(app, [this, &app] {
          setState(
            [&app](auto& self) { self.launch(*const_cast<App*>(&app)); });
        });
      }
    }
    return Wrap(widgets);
  }

  auto launcher(rmlib::AppContext& context) const {
    using namespace rmlib;

    return Cleared(Column(header(context), runningApps(), appList()));
  }

  auto build(rmlib::AppContext& context,
             const rmlib::BuildContext& /*unused*/) const {
    using namespace rmlib;

    auto ui = [&]() -> std::optional<DynamicWidget> {
      if (!visible) {
        return {};
      }

      if (background.has_value()) {
        return Center(Rotated(
          rotation,
          Sized(Image(*background), splash_size.width, splash_size.height)));
      }

      return Rotated(rotation, launcher(context));
    }();

    return GestureDetector(
      Hideable(std::move(ui)),
      Gestures{}
        .onKeyDown([this, &context](auto keyCode) {
          if (keyCode == KEY_POWER) {
            setState([&context](auto& self) { self.toggle(context); });
          }
        })
        .onAny([this]() { resetInactivity(); }));
  }

private:
  static bool sleep();

  void startTimer(rmlib::AppContext& context, int time = default_sleep_timeout);
  void stopTimer();

  void resetInactivity() const;
  void tick() const;

  void show();
  void hide(rmlib::AppContext* context);
  void toggle(rmlib::AppContext& context);

  void launch(App& app);
  void switchApp(pid_t pid);

  void onSignal();
  bool isRunning(pid_t pid) const;

  void readApps();
  void requestClients();

  void updateRotation(rmlib::AppContext& ctx);

  std::vector<App> apps;

  std::vector<ControlInterface::Client> fbClients;
  std::unordered_map<pid_t, SharedFB> fbBuffers;

  unistdpp::FD signalPipe;

  rmlib::TimerHandle sleepTimer;
  rmlib::TimerHandle inactivityTimer;

  rmlib::Rotation rotation = rmlib::Rotation::None;
  std::optional<rmlib::Canvas> background;

  int sleepCountdown = -1;
  mutable int inactivityCountdown = default_inactivity_timeout;
  bool visible = true;
};
