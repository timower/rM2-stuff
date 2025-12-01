#include "Launcher.h"

#include <unistdpp/file.h>

#ifdef __linux__
#include <systemd/sd-bus.h>
#endif

using namespace rmlib;

namespace {

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

unistdpp::FD writeFd;

void
signalHandler(int sig) {
  if (!writeFd.isValid()) {
    return;
  }
  writeFd.writeAll(sig).or_else([](auto err) {});
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

  unistdpp::fatalOnError(getWidget().ctlClient.setLauncher(getpid()),
                         "Failed to set launcher: ");

  getWidget()
    .ctlClient.getClients()
    .transform([this](auto clients) { fbClients = std::move(clients); })
    .or_else([](auto err) {
      std::cerr << "Error getting init clients: " << to_string(err) << "\n";
    });

  auto pipe = unistdpp::fatalOnError(unistdpp::pipe());
  writeFd = std::move(pipe.writePipe);
  signalPipe = std::move(pipe.readPipe);
  context.listenFd(signalPipe.fd, [&] { modify().onSignal(); });

  struct sigaction sigAct = {};
  sigAct.sa_flags = SA_RESTART; // make sure reading is restatart on switch.
  sigAct.sa_handler = signalHandler;
  sigaction(SIGUSR1, &sigAct, nullptr);
  sigaction(SIGCONT, &sigAct, nullptr);

  readApps();

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
  sleepTimer.disable();
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

  background.reset();

  if (auto err = getWidget().ctlClient.switchTo(getpid()); !err) {
    std::cerr << "Error switching: " << to_string(err.error()) << "\n";
  }
}

void
LauncherState::hide(rmlib::AppContext* context) {
  if (!visible) {
    return;
  }

  // TODO:
  // if (false) {
  //   switchApp(*current);
  // } else

  // sleep?
  if (context != nullptr) {
    startTimer(*context, 0);
  }
}

void
LauncherState::switchApp(pid_t pid) {
  auto err = getWidget().ctlClient.switchTo(pid);
  if (!err) {
    std::cerr << "Error switching: " << to_string(err.error()) << "\n";
  }
}

void
LauncherState::launch(App& app) {
  stopTimer();

  if (!app.launch()) {
    std::cerr << "Error launching " << app.description().command << std::endl;
    return;
  }

  if (auto icon = app.icon(); icon.has_value()) {
    background = *icon;
  }
}

void
LauncherState::onSignal() {
  auto sigOrErr = signalPipe.readAll<int>();
  if (!sigOrErr.has_value()) {
    return;
  }

  std::cerr << "Got signal: " << *sigOrErr << "\n";

  if (*sigOrErr == SIGUSR1) {
    stopTimer();
    visible = false;
  } else if (*sigOrErr == SIGCONT) {
    visible = true;
    background.reset();
    readApps();
    getWidget()
      .ctlClient.getClients()
      .transform([&](auto clients) { fbClients = std::move(clients); })
      .or_else([](auto err) {
        std::cerr << "Error getting clients: " << to_string(err) << "\n";
      });
  }
}

void
LauncherState::readApps() {
  auto appDescriptions = getWidget().appReader();

  // Update known apps.
  for (auto appIt = apps.begin(); appIt != apps.end();) {

    auto descIt = std::find_if(appDescriptions.begin(),
                               appDescriptions.end(),
                               [&app = *appIt](const auto& desc) {
                                 return desc.path == app.description().path;
                               });

    if (descIt == appDescriptions.end()) {
      // Remove old apps.
      appIt = apps.erase(appIt);
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

bool
LauncherState::isRunning(pid_t pid) const {
  return std::find_if(
           fbClients.begin(), fbClients.end(), [pid](const auto& client) {
             return client.pid == pid;
           }) != fbClients.end();
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
