#include "Launcher.h"

#include <unistdpp/pipe.h>

#include <sys/timerfd.h>

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

// Arms fd to fire once at the next wall-clock multiple of `interval` past
// the hour (e.g. :00/:15/:30/:45). Re-armed after every fire instead of
// using it_interval, since an interval on CLOCK_BOOTTIME only tracks
// elapsed monotonic time - it never resyncs to CLOCK_REALTIME, so any later
// wall-clock jump (NTP sync, manual date change, ...) would otherwise leave
// the schedule drifting away from the wall-clock boundaries forever.
unistdpp::Result<void>
armBatteryTimerFd(int fd, std::chrono::seconds interval) {
  const auto nowSecs = std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now().time_since_epoch());
  const auto firstFire =
    interval - std::chrono::seconds(nowSecs.count() % interval.count());

  const itimerspec spec{
    .it_interval = {},
    .it_value = { .tv_sec = firstFire.count(), .tv_nsec = 0 },
  };
  if (timerfd_settime(fd, 0, &spec, nullptr) == -1) {
    return tl::unexpected(unistdpp::getErrno());
  }

  return {};
}

// CLOCK_BOOTTIME_ALARM (needs CAP_WAKE_ALARM, held by root) wakes the device
// from suspend too, unlike a regular timerfd, so the cached battery
// percentage doesn't go stale for hours while sleeping.
unistdpp::Result<unistdpp::FD>
makeBatteryTimerFd(std::chrono::seconds interval) {
  auto fd = unistdpp::FD(timerfd_create(CLOCK_BOOTTIME_ALARM, TFD_CLOEXEC));
  if (!fd.isValid()) {
    return tl::unexpected(unistdpp::getErrno());
  }

  if (auto res = armBatteryTimerFd(fd.fd, interval); !res) {
    return tl::unexpected(res.error());
  }

  return fd;
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

  getWidget().ctlClient.setLauncher(getpid()).or_else([](auto err) {
    std::cerr << "Failed to set launcher: " << to_string(err) << "\n";
  });

  requestClients();

  auto pipe = unistdpp::fatalOnError(unistdpp::pipe());
  writeFd = std::move(pipe.writePipe);
  signalPipe = std::move(pipe.readPipe);
  signalFdHandle =
    context.listenFd(signalPipe.fd, [&] { modify().onSignal(context); });

  struct sigaction sigAct = {};
  sigAct.sa_flags = SA_RESTART; // make sure reading is restatart on switch.
  sigAct.sa_handler = signalHandler;
  sigaction(SIGUSR1, &sigAct, nullptr);
  sigaction(SIGCONT, &sigAct, nullptr);

  readApps();

  makeBatteryTimerFd(battery_poll_interval)
    .transform([this, &context](auto fd) {
      batteryTimerFd = std::move(fd);
      batteryFdHandle =
        context.listenFd(batteryTimerFd.fd, [this] { refreshBattery(true); });
    })
    .or_else([](auto err) {
      std::cerr << "Failed to create battery timer: "
                << unistdpp::to_string(err) << "\n";
    });

  // Make sure drawing finishes before shutdown on startup.
  context.doLater([this] { refreshBattery(false); });

  clockTimer = context.addTimer(
    std::chrono::minutes(1),
    [this] { setState([](auto& /*unused*/) {}); },
    std::chrono::minutes(1));

  takeInhibitorLock();

  if (auto fd = getWidget().power.sleepFd(); fd >= 0) {
    sleepFdHandle = context.listenFd(
      fd, [this, &context] { modify().onSleepFdReady(context); });
  }
  takeSleepLock();

  inactivityTimer = context.addTimer(
    std::chrono::minutes(1),
    [this, &context] { tickInactivity(context); },
    std::chrono::minutes(1));

  updateRotation(context);
  context.onDeviceUpdate(
    [this, &context] { modify().updateRotation(context); });
}

void
LauncherState::refreshBattery(bool clearTimer) const {
  if (clearTimer) {
    batteryTimerFd.readAll<uint64_t>().or_else([](auto /*unused*/) {});
    armBatteryTimerFd(batteryTimerFd.fd, battery_poll_interval)
      .or_else([](auto err) {
        std::cerr << "Failed to re-arm battery timer: "
                  << unistdpp::to_string(err) << "\n";
      });
    setState([](auto& /*unused*/) {});
  }

  auto battery = getWidget().power.getBattery();
  if (!battery.has_value() || battery->isCharging ||
      battery->percentage >= battery_shutdown_percentage) {
    return;
  }

  getWidget().power.powerOff();
}

void
LauncherState::onSleepFdReady(rmlib::AppContext& context) {
  // pollSleep() only ever returns a single transition, so a queued
  // true-then-false pair (e.g. after the process itself was frozen for the
  // actual suspend) needs draining in a loop to react to both in order.
  for (;;) {
    auto update = getWidget().power.pollSleep();
    if (!update.has_value()) {
      return;
    }

    if (update->sleeping) {
      onPrepareSleep(context);
    } else {
      onResume(context, update->wokenByUser);
    }
  }
}

void
LauncherState::onPrepareSleep(rmlib::AppContext& context) {
  if (std::holds_alternative<Visible>(visibility)) {
    // build() turns this into a synced GC16 refresh, guaranteeing
    // "Sleeping" is actually on the panel before we let suspend proceed -
    // regardless of what triggered it.
    sleepTimer.disable();
    sleepPhase = AboutToSuspend{};
  }

  // Deferred so the release lands after the *next* draw (which now reflects
  // the state above, if we just set it) rather than before it - see
  // AppContext::step()'s doAllLaters() placement. Detaches sleepLock's raw
  // fd rather than calling releaseSleepLock() later: a resume can arrive
  // (and re-acquire a fresh lock into the member) before this runs, and
  // closing by member reference would then release the wrong one. (Callback
  // must stay copyable for std::function, hence the raw int instead of
  // moving the FD itself.)
  auto fd = std::exchange(sleepLock.fd, unistdpp::FD::invalid_fd);
  context.doLater([fd] { unistdpp::FD{ fd }.close(); });
}

void
LauncherState::onResume(rmlib::AppContext& context, bool wokenByUser) {
  takeSleepLock();

  if (!std::holds_alternative<AboutToSuspend>(sleepPhase)) {
    // Not a suspend we reacted to (rocket wasn't on screen) - nothing else
    // to do.
    return;
  }

  if (wokenByUser) {
    resetInactivity();
    hide(nullptr);
    sleepPhase = Idle{};
  } else {
    // Retry sleeping if something else woke us.
    startTimer(context, retry_sleep_timeout);
  }
}

void
LauncherState::stopTimer() {
  sleepTimer.disable();
  sleepPhase = Idle{};
}

void
LauncherState::startTimer(rmlib::AppContext& context, int time) {
  sleepPhase = CountingDown{ time };
  sleepTimer.disable();
  sleepTimer = context.addTimer(
    std::chrono::seconds(1),
    [this, &context] { tick(context); },
    std::chrono::seconds(1));
}

void
LauncherState::tick(rmlib::AppContext& context) const {
  setState([&context](auto& self) {
    auto next = std::visit(
      overloaded{
        [&self, &context](const CountingDown& cd) -> std::optional<SleepPhase> {
          if (cd.secondsLeft > 1) {
            return SleepPhase{ CountingDown{ cd.secondsLeft - 1 } };
          }

          self.sleepNow(context);
          return std::nullopt;
        },
        [](const auto&) -> std::optional<SleepPhase> { return std::nullopt; },
      },
      self.sleepPhase);

    if (next) {
      self.sleepPhase = std::move(*next);
    }
  });
}

void
LauncherState::sleepNow(rmlib::AppContext& context) {
  // onPrepareSleep() is what actually moves to AboutToSuspend, once
  // PrepareForSleep(true) confirms the suspend is really happening.
  sleepTimer.disable();
  if (!getWidget().power.requestSuspend()) {
    // The request itself failed - retry, since no PrepareForSleep will
    // ever arrive to drive things otherwise.
    startTimer(context, retry_sleep_timeout);
  }
}

void
LauncherState::toggle(rmlib::AppContext& context) {
  clearSplash();

  if (std::holds_alternative<Visible>(visibility)) {
    const bool shouldStartTimer =
      !std::holds_alternative<CountingDown>(sleepPhase);
    stopTimer();
    hide(shouldStartTimer ? &context : nullptr);
  } else if (std::holds_alternative<Hidden>(visibility)) {
    show(context);
  }
  // Ignore repeated presses while a show request (PendingVisible) is in
  // flight.
}

void
LauncherState::show(rmlib::AppContext& context) {
  if (!std::holds_alternative<Hidden>(visibility)) {
    return;
  }

  pid_t returnTo = -1;
  const auto clientsOrErr = getWidget().ctlClient.getClients();
  if (clientsOrErr) {
    const auto it =
      std::find_if(clientsOrErr->begin(),
                   clientsOrErr->end(),
                   [](const auto& client) { return client.active; });
    returnTo = it == clientsOrErr->end() ? -1 : it->pid;
  } else {
    std::cerr << "Error getting clients: " << to_string(clientsOrErr.error())
              << "\n";
  }
  if (auto err = getWidget().ctlClient.switchTo(getpid()); !err) {
    std::cerr << "Error switching: " << to_string(err.error()) << "\n";
  }

  // `timeout` self-heals if the expected SIGCONT ack never arrives, so a
  // failed/dropped switch can't leave us stuck in PendingVisible forever.
  visibility = PendingVisible{
    returnTo,
    context.addTimer(show_timeout,
                     [this] {
                       setState([](auto& self) {
                         if (std::holds_alternative<PendingVisible>(
                               self.visibility)) {
                           std::cerr << "Timed out waiting to become visible\n";
                           self.visibility = Hidden{};
                         }
                       });
                     }),
  };
}

void
LauncherState::hide(rmlib::AppContext* context) {
  const auto* vis = std::get_if<Visible>(&visibility);
  if (vis == nullptr) {
    return;
  }

  if (vis->returnTo != -1) {
    switchApp(vis->returnTo);
  } else if (context != nullptr) {
    sleepNow(*context);
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

  auto icon = app.icon();
  if (!icon.has_value()) {
    return;
  }

  const auto pid = app.getLaunchPid();
  auto pidFd = app.takeLaunchPidFd();
  if (!pidFd.isValid()) {
    std::cerr << "Error opening pidfd for launched app\n";
    return;
  }

  auto pidFdHandle =
    ctx.listenFd(pidFd.fd, [this] { modify().onSplashAppExited(); });

  splashPhase = Splash{
    pid,
    *icon,
    std::move(pidFd),
    std::move(pidFdHandle),
    ctx.addTimer(splash_safety_timeout, [this] { modify().clearSplash(); }),
  };
}

void
LauncherState::clearSplash() {
  splashPhase = NoSplash{};
}

void
LauncherState::onSplashAppExited() {
  if (const auto* splash = std::get_if<Splash>(&splashPhase)) {
    std::cerr << "App " << splash->pid << " exited before becoming visible\n";
  }
  clearSplash();
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
    visibility = Hidden{};
    clearSplash();
  } else if (*sigOrErr == SIGCONT) {
    if (auto* pending = std::get_if<PendingVisible>(&visibility)) {
      visibility = Visible{ pending->returnTo };
      startTimer(context);
    } else {
      // Spurious/unrequested SIGCONT: become visible with no known app to
      // return to.
      visibility = Visible{ -1 };
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

void
LauncherState::releaseInhibitorLock() {
  inhibitorLock.close();
}

void
LauncherState::takeInhibitorLock() {
  getWidget()
    .power.acquireIdleLock()
    .transform([this](auto fd) { inhibitorLock = std::move(fd); })
    .or_else([](auto errc) {
      std::cerr << "Could not get inhibit lock: " << to_string(errc) << "\n";
    });
}

void
LauncherState::takeSleepLock() {
  getWidget()
    .power.acquireSleepLock()
    .transform([this](auto fd) { sleepLock = std::move(fd); })
    .or_else([](auto errc) {
      std::cerr << "Could not get sleep inhibit lock: " << to_string(errc)
                << "\n";
    });
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
LauncherState::tickInactivity(rmlib::AppContext& context) const {
  inactivityCountdown -= 1;
  if (inactivityCountdown == 0) {
    setState([&context](auto& self) {
      self.releaseInhibitorLock();
      if (std::holds_alternative<Hidden>(self.visibility)) {
        self.show(context);
      } else {
        self.startTimer(context);
      }
    });
  }
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
