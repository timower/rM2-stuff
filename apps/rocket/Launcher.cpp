#include "Launcher.h"
#include "Commands.h"

#include <Device.h>

#include <iostream>

#include <linux/input.h>
#include <unistd.h>

using namespace rmlib;
using namespace rmlib::input;

Launcher::Launcher()
  : cmdSocket([this](std::string_view cmd) -> std::string {
    auto result = doCommand(*this, cmd);
    if (result.isError()) {
      return "ERROR: " + result.getError().msg;
    }

    return *result;
  }) {}

/// Reads apps from draft files in `/etc/draft`
void
Launcher::readApps() {
  constexpr auto apps_path = "/etc/draft";

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
      appIt->description = *descIt;
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

App*
Launcher::getCurrentApp() {
  auto app = std::find_if(apps.begin(), apps.end(), [this](auto& app) {
    return app.description.path == currentAppPath;
  });

  if (app == apps.end()) {
    return nullptr;
  }

  return &*app;
}

void
Launcher::closeLauncher() {
  if (backupBuffer.has_value()) {
    // copy the backup buffer to the fb.
    copy(frameBuffer->canvas,
         backupBuffer->first,
         backupBuffer->second.canvas,
         backupBuffer->second.canvas.rect());

    const auto rect = backupBuffer->first + backupBuffer->second.canvas.rect();
    frameBuffer->doUpdate(rect, fb::Waveform::GC16Fast, fb::UpdateFlags::None);
    backupBuffer = std::nullopt;
  }

  state = State::Default;
  inputFds->touch.ungrab();

  if (auto* currentApp = getCurrentApp();
      currentApp != nullptr && currentApp->isPaused()) {
    currentApp->resume();
  }
}

void
Launcher::switchApp(App& app) {
  app.lastActivated = std::chrono::steady_clock::now();

  if (currentAppPath == app.description.path) {
    closeLauncher();
    return;
  }

  // Don't redraw as launching an app will redraw anyway.
  closeLauncher();

  // Pause the current app.
  if (auto* currentApp = getCurrentApp();
      currentApp != nullptr && currentApp->isRunning()) {
    currentApp->pause(MemoryCanvas(frameBuffer->canvas));
  }

  // resume or launch app
  if (app.isPaused()) {
    inputFds->touch.flood();
    app.resume(&*frameBuffer);
  } else if (!app.isRunning()) {
    if (!app.launch()) {
      std::cerr << "Error launching " << app.description.command << std::endl;
      return;
    }
  }

  currentAppPath = app.description.path;
}

void
Launcher::drawAppsLauncher() {
  constexpr auto name_text_size = 64;
  constexpr auto margin = 10;
  constexpr auto text_margin = 10;

  static constexpr auto kill_text = "[x]";
  static const auto kill_size = Canvas::getTextSize(kill_text, name_text_size);
  static const auto ind_size = Canvas::getTextSize(">", name_text_size);

  if (state == State::ShowingLauncher) {
    return;
  }

  readApps();

  if (apps.empty()) {
    std::cout << "No apps found\n";
    return;
  }

  if (auto* currentApp = getCurrentApp(); currentApp != nullptr) {
    currentApp->pause();
  }
  inputFds->touch.grab();

  // Find the width and height.
  const auto overlayRect = [this] {
    auto longestApp = std::max_element(
      apps.begin(), apps.end(), [](const auto& a, const auto& b) {
        return a.description.name.size() < b.description.name.size();
      });
    assert(longestApp != apps.end());

    auto size =
      Canvas::getTextSize(longestApp->description.name, name_text_size);
    size.x += kill_size.x + ind_size.x;

    int width = size.x + 2 * margin;
    int height = (name_text_size + text_margin) * int(apps.size());
    // size.y * apps.size() + text_margin * apps.size(); // - 1) + 3 * margin;

    auto topLeft = Point{ (frameBuffer->canvas.width() / 2) - (width / 2), 0 };
    auto bottomRight =
      Point{ (frameBuffer->canvas.width() / 2) + (width / 2), height };
    return Rect{ topLeft, bottomRight };
  }();

  std::cout << "Launcher rect: " << overlayRect << std::endl;
  backupBuffer = std::make_pair(overlayRect.topLeft,
                                MemoryCanvas(frameBuffer->canvas, overlayRect));

  // Draw background.
  frameBuffer->canvas.set(overlayRect, 0xffff);

  // Draw text
  int yoffset = 0; // 2 * margin;
  for (auto& app : apps) {
    auto displayName = app.description.name;
    if (app.description.path == currentAppPath) {
      displayName = '>' + displayName;
    } else if (app.isPaused()) {
      displayName = '*' + displayName;
    } else if (app.isRunning()) {
      displayName = '+' + displayName;
    }

    auto textSize = Canvas::getTextSize(displayName, name_text_size);

    int xoffset = (frameBuffer->canvas.width() / 2) - (textSize.x / 2);
    if (app.isRunning()) {
      xoffset -= (kill_size.x / 2) + margin / 2;
    }

    yoffset += text_margin / 2;

    auto position = Point{ xoffset, yoffset };

    frameBuffer->canvas.drawText(displayName, position, name_text_size);
    app.launchRect = { position, position + textSize };

    if (app.isRunning()) {
      auto killPosition = Point{ xoffset + textSize.x + margin / 2, yoffset };
      frameBuffer->canvas.drawText(kill_text, killPosition, name_text_size);

      app.killRect = { killPosition, killPosition + kill_size };
    }

    yoffset += name_text_size;
    yoffset += text_margin / 2;
    frameBuffer->canvas.drawLine({ overlayRect.topLeft.x, yoffset },
                                 { overlayRect.bottomRight.x, yoffset },
                                 /* black */ 0x0);
  }

  frameBuffer->canvas.drawLine(
    overlayRect.topLeft,
    { overlayRect.topLeft.x, overlayRect.bottomRight.y },
    /* black */ 0x0);

  frameBuffer->canvas.drawLine(
    { overlayRect.bottomRight.x, overlayRect.topLeft.y },
    overlayRect.bottomRight,
    /* black */ 0x0);

  frameBuffer->doUpdate(
    overlayRect, fb::Waveform::GC16Fast, fb::UpdateFlags::None);

  state = State::ShowingLauncher;
}

void
Launcher::handleLauncherTap(TapGesture tap) {
  if (tap.fingers > 1) {
    return;
  }

  auto launcherRect = backupBuffer->first + backupBuffer->second.canvas.rect();
  if (!launcherRect.contains(tap.position)) {
    closeLauncher();
    return;
  }

  for (auto& app : apps) {
    if (app.launchRect.contains(tap.position)) {
      switchApp(app);
      break;
    }

    if (app.isRunning() && app.killRect.contains(tap.position)) {
      std::cout << "Killing " << app.description.name << std::endl;
      app.stop();
      closeLauncher();
    }
  }
}

void
printGesture(const TapGesture& g) {
  std::cout << "Gesture Tap (" << g.fingers << "): " << g.position << std::endl;
}

void
printGesture(const PinchGesture& g) {
  std::cout << "Gesture Pinch\n";
}

void
printGesture(const SwipeGesture& g) {
  std::cout << "Gesture Swipe";

  switch (g.direction) {
    case SwipeGesture::Up:
      std::cout << " Up";
      break;
    case SwipeGesture::Down:
      std::cout << " Down";
      break;
    case SwipeGesture::Left:
      std::cout << " Left";
      break;
    case SwipeGesture::Right:
      std::cout << " Right";
      break;
  }

  std::cout << " fingers: " << g.fingers << std::endl;
}

template<typename Gesture>
void
Launcher::handleGesture(const Gesture& g) {
  printGesture(g);

  switch (state) {

    case State::ShowingLauncher:
      if constexpr (std::is_same_v<Gesture, TapGesture>) {
        handleLauncherTap(g);
        break;
      }
    case State::Default:
      // Default, look through config.
      for (const auto& action : config.actions) {
        if (action.config.matches(g)) {
          action.command();
          // doAction(action);
        }
      }
      break;
  }
}

OptError<>
Launcher::init() {
  inputFds.emplace(TRY(inputMgr.openAll()));

  readApps();

  std::cout << "Apps:" << std::endl;
  for (const auto& app : apps) {
    std::cout << " - " << app.description.name << std::endl;
  }

  auto fb = TRY(fb::FrameBuffer::open());
  frameBuffer.emplace(std::move(fb));

  frameBuffer->clear(); // Fill white.

  if (!cmdSocket.init()) {
    return Error{ "Couldn't open command socket" };
  }

  return NoError{};
}

void
Launcher::run() {
  auto lastSync = std::chrono::steady_clock::now();
  while (!shouldStop) {
    auto eventsAndFd = inputMgr.waitForInput(std::nullopt, cmdSocket.getFd());

    // Close stopped apps.
    std::vector<pid_t> curStoppedApps;
    std::swap(curStoppedApps, stoppedApps);
    for (const auto pid : curStoppedApps) {
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
        App* lastApp = nullptr;
        for (auto& app : apps) {
          if (app.runInfo.has_value() &&
              (lastApp == nullptr ||
               app.lastActivated > lastApp->lastActivated)) {
            lastApp = &app;
          }
        }

        if (lastApp != nullptr) {
          switchApp(*lastApp);
        } else {
          currentAppPath = "";
          frameBuffer->clear();
          drawAppsLauncher();
        }
      }
    }

    if (eventsAndFd.isError()) {
      std::cerr << "Error reading input: " << eventsAndFd.getError().msg
                << "\n";
    } else {
      auto [gestures, unhandledEvs] =
        gestureController.handleEvents(eventsAndFd->first);

      for (const auto& gesture : gestures) {
        std::visit([this](const auto& g) { handleGesture(g); }, gesture);
      }

      for (const auto& ev : unhandledEvs) {
        if (std::holds_alternative<KeyEvent>(ev)) {
          const auto& keyEv = std::get<KeyEvent>(ev);
          if (keyEv.keyCode == KEY_POWER && keyEv.type == KeyEvent::Release) {
            //  system("/sbin/rmmod brcmfmac");
            //  system("echo \"mem\" > /sys/power/state");
            //  system("/sbin/modprobe brcmfmac");
          }
        }
      }

      if (eventsAndFd->second[0]) {
        cmdSocket.process();
      }
    }

    // Sync gesture controller, TODO: is this still needed.
    auto now = std::chrono::steady_clock::now();
    if (now - lastSync > std::chrono::seconds(10)) {
      std::cerr << "Syncing" << std::endl;
      gestureController.sync(inputFds->touch);
      lastSync = now;
    }
  }
}
