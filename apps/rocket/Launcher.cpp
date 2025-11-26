#include "Launcher.h"

#include <unistdpp/file.h>

#ifdef __linux__
#include <systemd/sd-bus.h>
#endif

using namespace rmlib;

namespace {

constexpr std::array static_app_paths = { "/opt/etc/draft", "/etc/draft" };

#ifndef KEY_POWER
#define KEY_POWER 116
#endif

int
waitForSleep() {
#ifdef __linux__
  bool inSleep = false;
  int res = 0;
  sd_bus* bus = nullptr;
  res = sd_bus_open_system(&bus);
  if (res < 0) {
    std::cerr << "Error opening system bus: " << strerror(-res) << "\n";
    return res;
  }

  res = sd_bus_match_signal(
    bus,
    nullptr,
    "org.freedesktop.login1",
    "/org/freedesktop/login1",
    "org.freedesktop.login1.Manager",
    "PrepareForSleep",
    [](sd_bus_message* m, void* inSleep, sd_bus_error*) -> int {
      int sleeping;
      int res = sd_bus_message_read(m, "b", &sleeping);
      if (res < 0) {
        std::cerr << "Error reading message: " << strerror(-res) << "\n";
        return res;
      }

      std::cout << "Sleep got: " << sleeping << "\n";

      if (!sleeping) {
        // Waking up
        *(bool*)inSleep = true;
      }
      return 0;
    },
    &inSleep);
  if (res < 0) {
    std::cerr << "Error subscribing to signal: " << strerror(-res) << "\n";
    sd_bus_unref(bus);
    return res;
  }

  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message* reply = nullptr;
  res = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           "Suspend",
                           &error,
                           &reply,
                           "b",
                           /* interactive */ 1);
  if (res < 0) {
    std::cerr << "Error suspending: " << strerror(-res) << "\n";
    sd_bus_unref(bus);
    return res;
  }
  sd_bus_message_unref(reply);

  // Wait for suspend signal
  while (!inSleep) {
    res = sd_bus_process(bus, nullptr);
    if (res < 0) {
      std::cerr << "Error reading: " << strerror(-res) << "\n";
      break;
    }

    if (res > 0) {
      continue;
    }

    res = sd_bus_wait(bus, -1);
    if (res < 0) {
      std::cerr << "Error reading: " << strerror(-res) << "\n";
      break;
    }
  }

  sd_bus_error_free(&error);
  sd_bus_unref(bus);

  return 0;
#else
  return 1;
#endif
}

} // namespace

LauncherState
LauncherWidget::createState() {
  return LauncherState{};
}

void
LauncherState::init(rmlib::AppContext& context,
                    const rmlib::BuildContext& /*unused*/) {
  if (auto* key = context.getInputManager().getBaseDevices().key;
      key != nullptr) {
    key->grab();
  }

  fbCanvas = &context.getFbCanvas();
  inputManager = &context.getInputManager();

  readApps();

  context.listenFd(AppManager::getInstance().getWaitFD().fd, [this] {
    setState([](auto& self) { self.updateStoppedApps(); });
  });

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

  updateRotation(context);
  context.onDeviceUpdate(
    [this, &context] { modify().updateRotation(context); });
}

bool
LauncherState::sleep() {
  int res = waitForSleep();
  if (res == 0) {
    // Get the reason
    auto irq = unistdpp::readFile("/sys/power/pm_wakeup_irq");
    if (!irq.has_value()) {
      std::cout << "Error getting reason: " << unistdpp::to_string(irq.error())
                << std::endl;

      // If there is no irq it must be the user which pressed the button:
      return true;
    }
    std::cout << "Reason for wake irq: " << *irq << std::endl;
    return false;
  }

  return false;
}
void
LauncherState::stopTimer() {
  sleepTimer.disable();
  sleepCountdown = -1;
}

void
LauncherState::startTimer(rmlib::AppContext& context, int time) {
  sleepCountdown = time;
  sleepTimer = context.addTimer(
    std::chrono::seconds(time == 0 ? 0 : 1),
    [this] { tick(); },
    std::chrono::seconds(1));
}

void
LauncherState::tick() const {
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

void
LauncherState::toggle(rmlib::AppContext& context) {
  if (visible) {
    bool shouldStartTimer = sleepCountdown <= 0;
    stopTimer();
    hide(shouldStartTimer ? &context : nullptr);
  } else {
    startTimer(context);
    show();
  }
}

void
LauncherState::show() {
  if (visible) {
    return;
  }

  if (auto* current = getCurrentApp(); current != nullptr) {
    current->pause(MemoryCanvas(*fbCanvas));
    // pausing failed
    if (!current->isPaused()) {
      return;
    }
  }

  readApps();
  visible = true;
}

void
LauncherState::hide(rmlib::AppContext* context) {
  if (!visible) {
    return;
  }

  if (auto* current = getCurrentApp(); current != nullptr) {
    switchApp(*current);
  } else if (context != nullptr) {
    startTimer(*context, 0);
  }
}

App*
LauncherState::getCurrentApp() {
  auto app = std::find_if(apps.begin(), apps.end(), [this](auto& app) {
    return app.description().path == currentAppPath;
  });

  if (app == apps.end()) {
    return nullptr;
  }

  return &*app;
}

const App*
LauncherState::getCurrentApp() const {
  // NOLINTNEXTLINE
  return const_cast<LauncherState*>(this)->getCurrentApp();
}

void
LauncherState::switchApp(App& app) {
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
    app.resume();
  } else if (!app.isRunning()) {
    app.resetSavedFB();

    if (!app.launch()) {
      std::cerr << "Error launching " << app.description().command << std::endl;
      return;
    }
  }

  currentAppPath = app.description().path;
}

void
LauncherState::updateStoppedApps() {
  AppManager::getInstance().update();

  bool shouldShow = false;
  if (const auto* current = getCurrentApp();
      current != nullptr && !current->isRunning()) {
    currentAppPath = "";
    shouldShow = true;
  }

  apps.erase(std::remove_if(apps.begin(),
                            apps.end(),
                            [](const App& app) {
                              return app.shouldRemoveOnExit() &&
                                     !app.isRunning();
                            }),
             apps.end());

  if (shouldShow) {
    show();
  }
}

void
LauncherState::readApps() {
  const static auto app_paths = [] {
    std::vector<std::string> paths;
    std::transform(static_app_paths.begin(),
                   static_app_paths.end(),
                   std::back_inserter(paths),
                   [](const auto* str) { return std::string(str); });

    if (const auto* home = getenv("HOME"); home != nullptr) {
      paths.push_back(std::string(home) + "/.config/draft");
    }

    return paths;
  }();

  std::vector<AppDescription> appDescriptions;
  for (const auto& appsPath : app_paths) {
    auto decriptions = readAppFiles(appsPath);
    std::move(decriptions.begin(),
              decriptions.end(),
              std::back_inserter(appDescriptions));
  }

  // Update known apps.
  for (auto appIt = apps.begin(); appIt != apps.end();) {

    auto descIt = std::find_if(appDescriptions.begin(),
                               appDescriptions.end(),
                               [&app = *appIt](const auto& desc) {
                                 return desc.path == app.description().path;
                               });

    // Remove old apps.
    if (descIt == appDescriptions.end()) {
      if (!appIt->isRunning()) {
        appIt = apps.erase(appIt);
        continue;
      }

      // Defer removing until exit.
      appIt->setRemoveOnExit();

    } else {

      // Update existing apps.
      appIt->updateDescription(std::move(*descIt));
      appDescriptions.erase(descIt);
    }

    ++appIt;
  }

  // Any left over descriptions are new.
  for (auto& desc : appDescriptions) {
    apps.emplace_back(std::move(desc));
  }

  std::sort(apps.begin(), apps.end(), [](const auto& app1, const auto& app2) {
    return app1.description().path < app2.description().path;
  });
}

void
LauncherState::resetInactivity() const {
  inactivityCountdown = default_inactivity_timeout;
}

void
LauncherState::updateRotation(rmlib::AppContext& ctx) {
  rotation = ctx.getInputManager().getBaseDevices().pogoKeyboard != nullptr
               ? Rotation::Clockwise
               : Rotation::None;
}
