#include "Launcher.h"

#include <systemdpp/sdbus.h>
#include <unistdpp/file.h>

using namespace rmlib;

namespace {

#ifndef KEY_POWER
#define KEY_POWER 116
#endif

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

  requestClients();

  auto pipe = unistdpp::fatalOnError(unistdpp::pipe());
  writeFd = std::move(pipe.writePipe);
  signalPipe = std::move(pipe.readPipe);
  context.listenFd(signalPipe.fd, [&] { modify().onSignal(context); });

  struct sigaction sigAct = {};
  sigAct.sa_flags = SA_RESTART; // make sure reading is restatart on switch.
  sigAct.sa_handler = signalHandler;
  sigaction(SIGUSR1, &sigAct, nullptr);
  sigaction(SIGCONT, &sigAct, nullptr);

  readApps();

  takeInhibitorLock();
  inactivityTimer = context.addTimer(
    std::chrono::minutes(1),
    [this, &context] {
      inactivityCountdown -= 1;
      if (inactivityCountdown == 0) {
        releaseInhibitorLock();
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

void
LauncherState::releaseInhibitorLock() {
  inhibitorLock.close();
}

void
LauncherState::takeInhibitorLock() {
  systemdpp::getInhibitLock()
    .transform([this](auto fd) { inhibitorLock = std::move(fd); })
    .or_else([](auto errc) {
      std::cerr << "Could not get inhibit lock: " << to_string(errc) << "\n";
    });
}

bool
LauncherState::sleep() {
  if (systemdpp::waitForSleep()) {
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
  background.reset();
  if (visible) {
    bool shouldStartTimer = sleepCountdown <= 0;
    stopTimer();
    hide(shouldStartTimer ? &context : nullptr);
  } else {
    // Deferred to onSignal()'s SIGCONT - show()'s switchTo() can be held up
    // server-side until xochitl reports idle, so starting the countdown here
    // would eat into it before the launcher is even visible.
    startSleepTimerOnShow = true;
    show();
  }
}

void
LauncherState::show() {
  if (visible) {
    return;
  }

  const auto clientsOrErr = getWidget().ctlClient.getClients();
  if (clientsOrErr) {
    const auto it =
      std::find_if(clientsOrErr->begin(),
                   clientsOrErr->end(),
                   [](const auto& client) { return client.active; });
    lastActive = it == clientsOrErr->end() ? -1 : it->pid;
  } else {
    std::cerr << "Error getting clients: " << to_string(clientsOrErr.error())
              << "\n";
  }
  if (auto err = getWidget().ctlClient.switchTo(getpid()); !err) {
    std::cerr << "Error switching: " << to_string(err.error()) << "\n";
  }
}

void
LauncherState::hide(rmlib::AppContext* context) {
  if (!visible) {
    return;
  }

  if (lastActive != -1) {
    switchApp(lastActive);
  } else if (context != nullptr) {
    // sleep?
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
LauncherState::launch(rmlib::AppContext& ctx, App& app) {
  stopTimer();

  if (!app.launch()) {
    std::cerr << "Error launching " << app.description().command << std::endl;
    return;
  }

  if (auto icon = app.icon(); icon.has_value()) {
    background = *icon;
    backgroundTimer = ctx.addTimer(std::chrono::seconds(3),
                                   [this] { modify().background.reset(); });
  }
}

void
LauncherState::onSignal(rmlib::AppContext& context) {
  auto sigOrErr = signalPipe.readAll<int>();
  if (!sigOrErr.has_value()) {
    return;
  }

  std::cerr << "Got signal: " << *sigOrErr << "\n";

  if (*sigOrErr == SIGUSR1) {
    stopTimer();
    visible = false;
    background.reset();
  } else if (*sigOrErr == SIGCONT) {
    visible = true;

    if (startSleepTimerOnShow) {
      startSleepTimerOnShow = false;
      startTimer(context);
    }

    readApps();
    requestClients();
  }
}

void
LauncherState::requestClients() {
  auto clients = getWidget().ctlClient.getClients();
  if (!clients) {
    std::cerr << "Error getting clients: " << to_string(clients.error())
              << "\n";
    return;
  }
  fbClients = std::move(*clients);

  for (const auto& client : fbClients) {
    if (fbBuffers.count(client.pid) != 0) {
      continue;
    }
    auto fd = getWidget().ctlClient.getFramebuffer(client.pid);
    if (!fd) {
      std::cerr << "Error getting fb: " << to_string(fd.error()) << "\n";
      continue;
    }

    auto& fb = fbBuffers[client.pid];
    fb.setFD(unistdpp::FD(*fd), client.format);
    fb.mmap();
  }

  // Remove old buffers.
  for (auto it = fbBuffers.begin(); it != fbBuffers.end();) {
    auto clientIt =
      std::find_if(fbClients.begin(), fbClients.end(), [&](const auto& client) {
        return client.pid == it->first;
      });
    if (clientIt == fbClients.end()) {
      it = fbBuffers.erase(it);
    } else {
      ++it;
    }
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
      continue;
    }

    // Update existing apps.
    appIt->updateDescription(std::move(*descIt));
    appDescriptions.erase(descIt);

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
  if (!inhibitorLock.isValid()) {
    // const cast here as we don't want to trigger a rebuild.
    const_cast<LauncherState*>(this)->takeInhibitorLock();
  }
  inactivityCountdown = default_inactivity_timeout;
}

void
LauncherState::updateRotation(rmlib::AppContext& ctx) {
  rotation = ctx.getInputManager().getBaseDevices().pogoKeyboard != nullptr
               ? Rotation::Clockwise
               : Rotation::None;
}

void
LauncherState::onKey(rmlib::AppContext& ctx, int keycode, bool down) const {
  if (down && keycode == KEY_POWER) {
    setState([&ctx](auto& self) { self.toggle(ctx); });
    return;
  }

  if (keycode == KEY_LEFTALT) {
    // const cast here as we don't want to trigger a rebuild.
    const_cast<LauncherState*>(this)->modPressed = down;
    return;
  }

  // Only hande key up with mod pressed.
  if (down || !modPressed) {
    return;
  }

  int offset = 0;
  if (keycode == KEY_LEFT) {
    offset = -1;
  } else if (keycode == KEY_RIGHT) {
    offset = 1;
  } else {
    return;
  }

  const auto clientsOrErr = getWidget().ctlClient.getClients();
  if (!clientsOrErr) {
    return;
  }
  const auto& clients = *clientsOrErr;
  auto it = std::find_if(clients.begin(),
                         clients.end(),
                         [](const auto& client) { return client.active; });
  if (it == clients.end()) {
    return;
  }

  auto idx = std::distance(clients.begin(), it);
  idx = (idx + offset) % clients.size();
  getWidget().ctlClient.switchTo(clients[idx].pid);
}
