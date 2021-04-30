#include "App.h"

#include <csignal>
#include <iostream>
#include <sys/wait.h>

#include <Device.h>
#include <UI.h>

using namespace rmlib;

namespace {

#ifndef KEY_POWER
#define KEY_POWER 116
#endif

constexpr auto config_path_suffix = ".config/rocket/config";

std::vector<pid_t> stoppedChildren;
std::function<void()> stopCallback;

void
cleanup(int signal) {
  pid_t childPid = 0;
  while ((childPid = waitpid(static_cast<pid_t>(-1), nullptr, WNOHANG)) > 0) {
    std::cout << "Exited: " << childPid << std::endl;
    stoppedChildren.push_back(childPid);
  }

  if (stopCallback) {
    stopCallback();
  }
}

template<typename Child>
class Hideable;

template<typename Child>
class HideableRenderObject : public SingleChildRenderObject<Hideable<Child>> {
public:
  HideableRenderObject(const Hideable<Child>& widget)
    : SingleChildRenderObject<Hideable<Child>>(
        widget,
        widget.child.has_value() ? widget.child->createRenderObject()
                                 : nullptr) {}

  Size doLayout(const Constraints& constraints) override {
    if (!this->widget->child.has_value()) {
      return constraints.min;
    }
    return this->child->layout(constraints);
  }

  void update(const Hideable<Child>& newWidget) {
    auto wasVisible = this->widget->child.has_value();
    this->widget = &newWidget;
    if (this->widget->child.has_value()) {
      if (this->child == nullptr) {
        this->child = this->widget->child->createRenderObject();
      } else {
        this->widget->child->update(*this->child);
      }

      if (!wasVisible) {
        doRefresh = true;
        this->markNeedsDraw();
        // TODO: why!!??
        this->markNeedsLayout();
      }
    } else if (this->widget->background != nullptr && wasVisible) {
      // Don't mark the children!
      RenderObject::markNeedsDraw();
    }
  }

  void handleInput(const rmlib::input::Event& ev) override {
    if (this->widget->child.has_value()) {
      this->child->handleInput(ev);
    }
  }

protected:
  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    if (!this->widget->child.has_value()) {
      if (this->widget->background != nullptr) {
        const auto offset =
          (rect.size() - this->widget->background->rect().size()) / 2;
        copy(canvas,
             offset.toPoint(),
             *this->widget->background,
             this->widget->background->rect());
        return UpdateRegion{ rect };
      }

      return UpdateRegion{};
    }

    auto result = this->child->draw(rect, canvas);
    if (doRefresh) {
      doRefresh = false;
      result.waveform = fb::Waveform::GC16;
      result.flags = static_cast<fb::UpdateFlags>(fb::UpdateFlags::FullRefresh |
                                                  fb::UpdateFlags::Sync);
    }
    return result;
  }

private:
  Size childSize;
  bool doRefresh = false;
};

template<typename Child>
class Hideable : public Widget<HideableRenderObject<Child>> {
private:
public:
  Hideable(std::optional<Child> child, const Canvas* background = nullptr)
    : child(std::move(child)), background(background) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<HideableRenderObject<Child>>(*this);
  }

private:
  friend class HideableRenderObject<Child>;
  std::optional<Child> child;
  const Canvas* background;
};

const auto missingImage = [] {
  auto mem = MemoryCanvas(128, 128, 2);
  mem.canvas.set(0xaa);
  return mem;
}();

class RunningAppWidget : public StatelessWidget<RunningAppWidget> {
public:
  RunningAppWidget(const App& app,
                   Callback onTap,
                   Callback onKill,
                   bool isCurrent)
    : app(app)
    , onTap(std::move(onTap))
    , onKill(std::move(onKill))
    , isCurrent(isCurrent) {}

  auto build(AppContext&, const BuildContext&) const {
    const Canvas& canvas =
      app.savedFb.has_value() ? app.savedFb->canvas : missingImage.canvas;

    return Container(
      Column(GestureDetector(Sized(Image(canvas), 234, 300),
                             Gestures{}.OnTap(onTap)),
             Row(Text(app.description.name), Button("X", onKill))),
      Insets::all(isCurrent ? 1 : 2),
      Insets::all(isCurrent ? 2 : 1),
      Insets::all(2));
  }

private:
  const App& app;
  Callback onTap;
  Callback onKill;
  bool isCurrent;
};

class AppWidget : public StatelessWidget<AppWidget> {
public:
  AppWidget(const App& app, Callback onLaunch)
    : app(app), onLaunch(std::move(onLaunch)) {}

  auto build(AppContext&, const BuildContext&) const {
    const Canvas& canvas = app.description.iconImage.has_value()
                             ? app.description.iconImage->canvas
                             : missingImage.canvas;
    return Container(GestureDetector(Column(Sized(Image(canvas), 128, 128),
                                            Text(app.description.name)),
                                     Gestures{}.OnTap(onLaunch)),
                     Insets::all(2),
                     Insets::all(1),
                     Insets::all(2));
  }

private:
  const App& app;
  Callback onLaunch;
};

class LauncherState;

class LauncherWidget : public StatefulWidget<LauncherWidget> {
public:
  LauncherState createState() const;
};

constexpr auto default_sleep_timeout = 10;
constexpr auto retry_sleep_timeout = 8;
constexpr auto default_inactivity_timeout = 20;

class LauncherState : public StateBase<LauncherWidget> {
public:
  ~LauncherState() {
    stopCallback = nullptr; // TODO: stop all apps
  }

  void init(AppContext& context, const BuildContext&) {
    context.getInputManager().getBaseDevices()->key.grab();

    fbCanvas = &context.getFbCanvas();
    touchDevice = &context.getInputManager().getBaseDevices()->touch;

    readApps(); // TODO: do this on turning visible

    assert(stopCallback == nullptr);
    stopCallback = [this, &context] {
      context.doLater(
        [this] { setState([](auto& self) { self.updateStoppedApps(); }); });
    };

    inactivityTimer = context.addTimer(
      std::chrono::minutes(1),
      [this, &context] {
        inactivityCountdown -= 1;
        if (inactivityCountdown == 0) {
          resetInactivity();
          setState([&context](auto& self) {
            self.startTimer(context);
            self.show();
          });
        }
      },
      std::chrono::minutes(1));
  }

  auto header(AppContext& context) const {
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
      } else if (sleepCountdown == 0) {
        // TODO: make hideable?
        return Button("...", [] {});
      } else {
        return Button("Sleep", [this, &context] {
          setState([&context](auto& self) { self.startTimer(context, 0); });
        });
      }
    }();

    return Center(Padding(
      Column(Padding(Text(text, 2 * default_text_size), Insets::all(10)),
             button),
      Insets::all(50)));
  }

  auto runningApps() const {
    std::vector<RunningAppWidget> widgets;
    for (const auto& app : apps) {
      if (app.isRunning()) {
        widgets.emplace_back(
          app,
          [this, &app] {
            setState(
              [&app](auto& self) { self.switchApp(*const_cast<App*>(&app)); });
          },
          [this, &app] {
            setState([&app](auto& self) {
              std::cout << "stopping " << app.description.name << std::endl;
              const_cast<App*>(&app)->stop();
              self.stopTimer();
            });
          },
          app.description.path == currentAppPath);
      }
    }
    return Wrap(widgets);
  }

  auto appList() const {
    std::vector<AppWidget> widgets;
    for (const auto& app : apps) {
      if (!app.isRunning()) {
        widgets.emplace_back(app, [this, &app] {
          setState(
            [&app](auto& self) { self.switchApp(*const_cast<App*>(&app)); });
        });
      }
    }
    return Wrap(widgets);
  }

  auto launcher(AppContext& context) const {
    return Cleared(Column(header(context), runningApps(), appList()));
  }

  auto build(AppContext& context, const BuildContext&) const {
    const Canvas* background = nullptr;
    if (auto* currentApp = getCurrentApp(); currentApp != nullptr) {
      if (currentApp->savedFb.has_value()) {
        background = &currentApp->savedFb->canvas;
      } else if (currentApp->description.iconImage.has_value()) {
        background = &currentApp->description.iconImage->canvas;
      }
    }

    auto ui = Hideable(
      visible ? std::optional(launcher(context)) : std::nullopt, background);

    return GestureDetector(
      ui,
      Gestures{}
        .OnKeyDown([this, &context](auto keyCode) {
          if (keyCode == KEY_POWER) {
            setState([&context](auto& self) { self.toggle(context); });
          }
        })
        .OnAny([this]() { resetInactivity(); }));
  }

private:
  bool sleep() {
    system("/sbin/rmmod brcmfmac");
    int res = system("echo \"mem\" > /sys/power/state");
    system("/sbin/modprobe brcmfmac");

    if (res == 0) {
      // Get the reason
      auto irq = rmlib::device::readFile("/sys/power/pm_wakeup_irq");
      if (irq.isError()) {
        std::cout << "Error getting reason: " << irq.getError().msg
                  << std::endl;

        // If there is no irq it must be the user which pressed the button:
        return true;

      } else {
        std::cout << "Reason for wake irq: " << *irq << std::endl;
        return false;
      }
    }

    return false;
  }

  void stopTimer() {
    sleepTimer.disable();
    sleepCountdown = -1;
  }

  void startTimer(AppContext& context, int time = default_sleep_timeout) {
    sleepCountdown = time;
    sleepTimer = context.addTimer(
      std::chrono::seconds(time == 0 ? 0 : 1),
      [this] { tick(); },
      std::chrono::seconds(1));
  }

  void tick() const {
    setState([](auto& self) {
      self.sleepCountdown -= 1;

      if (self.sleepCountdown == -1) {
        if (self.sleep()) {
          // If the user pressed the button, stop the timer and return to the
          // current app.
          self.resetInactivity();
          self.sleepTimer.disable();
          self.hide(nullptr);
        } else {
          // Retry sleeping if failed or something else woke us.
          self.sleepCountdown = retry_sleep_timeout;
        }
      }
    });
  }

  void toggle(AppContext& context) {
    if (visible) {
      bool shouldStartTimer = sleepCountdown <= 0;
      stopTimer();
      hide(shouldStartTimer ? &context : nullptr);
    } else {
      startTimer(context);
      show();
    }
  }

  void show() {
    if (visible) {
      return;
    }

    if (auto* current = getCurrentApp(); current != nullptr) {
      current->pause(MemoryCanvas(*fbCanvas));
    }

    readApps();
    visible = true;
  }

  void hide(AppContext* context) {
    if (!visible) {
      return;
    }

    if (auto* current = getCurrentApp(); current != nullptr) {
      switchApp(*current);
    } else if (context != nullptr) {
      startTimer(*context, 0);
    }
  }

  App* getCurrentApp() {
    auto app = std::find_if(apps.begin(), apps.end(), [this](auto& app) {
      return app.description.path == currentAppPath;
    });

    if (app == apps.end()) {
      return nullptr;
    }

    return &*app;
  }

  const App* getCurrentApp() const {
    return const_cast<LauncherState*>(this)->getCurrentApp();
  }

  void switchApp(App& app) {
    app.lastActivated = std::chrono::steady_clock::now();

    visible = false;
    stopTimer();

    // Pause the current app.
    if (auto* currentApp = getCurrentApp(); currentApp != nullptr &&
                                            currentApp->isRunning() &&
                                            !currentApp->isPaused()) {
      currentApp->pause();
    }

    // resume or launch app
    if (app.isPaused()) {
      if (touchDevice) {
        touchDevice->flood();
      }
      app.resume();
    } else if (!app.isRunning()) {
      app.savedFb = std::nullopt;

      if (!app.launch()) {
        std::cerr << "Error launching " << app.description.command << std::endl;
        return;
      }
    }

    currentAppPath = app.description.path;
  }

  void updateStoppedApps() {
    std::vector<pid_t> currentStoppedApps;
    std::swap(currentStoppedApps, stoppedChildren);

    for (const auto pid : currentStoppedApps) {
      auto app = std::find_if(apps.begin(), apps.end(), [pid](auto& app) {
        return app.isRunning() && app.runInfo->pid == pid;
      });

      if (app == apps.end()) {
        continue;
      }

      const auto isCurrent = app->description.path == currentAppPath;

      if (app->runInfo->shouldRemove) {
        apps.erase(app);
      } else {
        app->runInfo = std::nullopt;
      }

      if (isCurrent) {
        currentAppPath = "";
        show();
      }
    }
  }

  void readApps() {
#ifdef EMULATE
    const auto apps_path = [] {
      const auto* home = getenv("HOME");
      return std::string(home) + "/.config/draft";
    }();
#else
    constexpr auto apps_path = "/etc/draft";
#endif

    auto appDescriptions = readAppFiles(apps_path);

    // Update known apps.
    for (auto appIt = apps.begin(); appIt != apps.end();) {

      auto descIt = std::find_if(appDescriptions.begin(),
                                 appDescriptions.end(),
                                 [&app = *appIt](const auto& desc) {
                                   return desc.path == app.description.path;
                                 });

      // Remove old apps.
      if (descIt == appDescriptions.end()) {
        if (!appIt->isRunning()) {
          appIt = apps.erase(appIt);
          continue;
        } else {
          // Defer removing until exit.
          appIt->runInfo->shouldRemove = true;
        }
      } else {

        // Update existing apps.
        appIt->description = std::move(*descIt);
        appDescriptions.erase(descIt);
      }

      ++appIt;
    }

    // Any left over descriptions are new.
    for (auto& desc : appDescriptions) {
      apps.emplace_back(std::move(desc));
    }

    std::sort(apps.begin(), apps.end(), [](const auto& app1, const auto& app2) {
      return app1.description.path < app2.description.path;
    });
  }

  void resetInactivity() const {
    inactivityCountdown = default_inactivity_timeout;
  }

  std::vector<App> apps;
  std::string currentAppPath;

  std::optional<MemoryCanvas> backupBuffer;

  TimerHandle sleepTimer;
  TimerHandle inactivityTimer;

  const Canvas* fbCanvas = nullptr;
  input::InputDeviceBase* touchDevice = nullptr;

  int sleepCountdown = -1;
  mutable int inactivityCountdown = default_inactivity_timeout;
  bool visible = true;
};

LauncherState
LauncherWidget::createState() const {
  return LauncherState{};
}

} // namespace

int
main(int argc, char* argv[]) {

  std::signal(SIGCHLD, cleanup);

  runApp(LauncherWidget());

  auto fb = fb::FrameBuffer::open();
  if (!fb.isError()) {
    fb->canvas.set(white);
    fb->doUpdate(
      fb->canvas.rect(), fb::Waveform::GC16Fast, fb::UpdateFlags::None);
  }

  return EXIT_SUCCESS;
}
