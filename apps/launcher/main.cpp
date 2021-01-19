#include "App.h"

#include <Device.h>
#include <FrameBuffer.h>
#include <Input.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <csignal>
#include <fstream>
#include <iostream>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

using namespace rmlib;
using namespace rmlib::input;

namespace {

struct GestureConfig {
  enum Type { Swipe, Pinch, Tap };
  enum Action { ShowApps, Launch, NextRunning, PrevRunning };

  Type type;
  std::variant<PinchGesture::Direction, SwipeGesture::Direction> direction;
  int fingers;

  Action action;
  std::string command;

  bool matches(const SwipeGesture& g) const {
    return type == GestureConfig::Swipe &&
           std::get<SwipeGesture::Direction>(direction) == g.direction &&
           fingers == g.fingers;
  }

  bool matches(const PinchGesture& g) const {
    return type == GestureConfig::Pinch &&
           std::get<PinchGesture::Direction>(direction) == g.direction &&
           fingers == g.fingers;
  }

  bool matches(const TapGesture& g) const {
    return type == GestureConfig::Tap && g.fingers == fingers;
  }
};

struct Config {
  std::vector<GestureConfig> gestures;
};

enum class State { Default, ShowingLauncher };

struct Launcher {
  // Members
  std::vector<App> apps;
  std::string currentAppPath;
  State state = State::Default;

  std::optional<fb::FrameBuffer> frameBuffer;
  InputManager inputMgr;

  std::optional<std::pair<Point, MemoryCanvas>> backupBuffer;

  GestureController gestureController;

  Config config;

  int init();
  void run();

  void readApps();
  App* getCurrentApp();

  void closeDialogs();

  void switchApp(App& app);

  void drawAppsLauncher();

  void handleLauncherTap(TapGesture tap);

  void doAction(const GestureConfig& config);

  template<typename Gesture>
  void handleGesture(const Config& config, const Gesture& g);
};

bool
endsWith(std::string_view a, std::string_view end) {
  if (a.size() < end.size()) {
    return false;
  }

  return a.substr(a.size() - end.size()) == end;
}

/// Reads apps from draft files in `/etc/draft`
void
Launcher::readApps() {
  constexpr auto path = "/etc/draft";
  const auto paths = device::listDirectory(path);

  for (const auto& path : paths) {
    if (!endsWith(path, ".draft")) {
      std::cerr << "skipping non draft file: " << path << std::endl;
      continue;
    }

    auto appDesc = AppDescription::read(path);
    if (!appDesc.has_value()) {
      std::cerr << "error parsing file: " << path << std::endl;
    }

    apps.emplace_back(std::move(*appDesc));
  }
}

const Config default_config = { {
  { GestureConfig::Swipe, SwipeGesture::Down, 2, GestureConfig::ShowApps, "" },

  { GestureConfig::Swipe,
    SwipeGesture::Right,
    2,
    GestureConfig::NextRunning,
    "" },

  { GestureConfig::Swipe,
    SwipeGesture::Left,
    2,
    GestureConfig::PrevRunning,
    "" },
} };

std::optional<Config>
readConfg() {
  return default_config;
}

pid_t
runCommand(std::string_view cmd) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("Error launching");
    return -1;
  }

  if (pid > 0) {
    // Parent process, pid is the child pid
    return pid;
  }

  std::cout << "Running: " << cmd << std::endl;
  setpgid(0, 0);
  execlp("/bin/sh", "/bin/sh", "-c", cmd.data(), nullptr);
  perror("Error running process");
  return -1;
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
Launcher::closeDialogs() {
  if (backupBuffer.has_value()) {
    // copy the backup buffer to the fb.
    copy(frameBuffer->canvas,
         backupBuffer->first,
         backupBuffer->second.canvas,
         backupBuffer->second.canvas.rect());

    auto rect = backupBuffer->first + backupBuffer->second.canvas.rect();
    frameBuffer->doUpdate(rect, fb::Waveform::GC16Fast, fb::UpdateFlags::None);
  }

  backupBuffer = std::nullopt;
  state = State::Default;
  inputMgr.ungrab();
}

void
Launcher::switchApp(App& app) {
  app.lastActivated = std::chrono::steady_clock::now();

  if (currentAppPath == app.description.path) {
    closeDialogs();
    return;
  }

  // Don't redraw as launching an app will redraw anyway.
  closeDialogs();

  // pause the current app.
  if (auto* currentApp = getCurrentApp();
      currentApp != nullptr && currentApp->runInfo.has_value()) {
    kill(-currentApp->runInfo->pid, SIGSTOP);
    currentApp->runInfo->paused = true;
    currentApp->savedFb = MemoryCanvas(frameBuffer->canvas);
  }

  // resume or launch app
  if (app.runInfo.has_value()) {
    if (app.runInfo->paused) {
      if (app.savedFb.has_value()) {
        copy(frameBuffer->canvas,
             { 0, 0 },
             app.savedFb->canvas,
             frameBuffer->canvas.rect());
        frameBuffer->doUpdate(frameBuffer->canvas.rect(),
                              fb::Waveform::GC16Fast,
                              fb::UpdateFlags::FullRefresh);
        app.savedFb.reset();
      }

      inputMgr.flood();
      kill(-app.runInfo->pid, SIGCONT);
      app.runInfo->paused = false;
    } else {
      std::cerr << "App already running" << std::endl;
    }
  } else {
    auto pid = runCommand(app.description.command);
    if (pid == -1) {
      std::cerr << "Error running " << app.description.command << std::endl;
      return;
    }

    app.runInfo = AppRunInfo{ pid, /* paused */ false };
  }

  currentAppPath = app.description.path;
}

void
Launcher::drawAppsLauncher() {
  constexpr auto name_size = 64;
  constexpr auto margin = 10;
  constexpr auto text_margin = 10;

  static constexpr auto kill_text = "[x]";
  static const auto kill_size = Canvas::getTextSize(kill_text, name_size);
  static const auto ind_size = Canvas::getTextSize(">", name_size);

  if (state == State::ShowingLauncher) {
    return;
  }

  closeDialogs();

  if (apps.empty()) {
    return;
  }

  inputMgr.grab();

  // Find the width and height.
  const auto overlayRect = [this] {
    auto longestApp = std::max_element(
      apps.begin(), apps.end(), [](const auto& a, const auto& b) {
        return a.description.name.size() < b.description.name.size();
      });
    assert(longestApp != apps.end());

    auto size = Canvas::getTextSize(longestApp->description.name, name_size);
    size.x += kill_size.x + ind_size.x;

    int width = size.x + 2 * margin;
    int height = (name_size + text_margin) * int(apps.size());
    // size.y * apps.size() + text_margin * apps.size(); // - 1) + 3 * margin;

    auto topLeft = Point{ (frameBuffer->canvas.width / 2) - (width / 2), 0 };
    auto bottomRight =
      Point{ (frameBuffer->canvas.width / 2) + (width / 2), height };
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
    auto displayName = app.getDisplayName();
    auto textSize = Canvas::getTextSize(displayName, name_size);

    int xoffset = (frameBuffer->canvas.width / 2) - (textSize.x / 2);
    if (app.runInfo.has_value()) {
      xoffset -= (kill_size.x / 2) + margin / 2;
    }

    yoffset += text_margin / 2;

    auto position = Point{ xoffset, yoffset };

    frameBuffer->canvas.drawText(displayName, position, name_size);
    app.launchRect = { position, position + textSize };

    if (app.runInfo.has_value()) {
      auto killPosition = Point{ xoffset + textSize.x + margin / 2, yoffset };
      frameBuffer->canvas.drawText(kill_text, killPosition, name_size);

      app.killRect = { killPosition, killPosition + kill_size };
    }

    yoffset += name_size;
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
    closeDialogs();
    return;
  }

  for (auto& app : apps) {
    if (app.launchRect.contains(tap.position)) {
      switchApp(app);
      break;
    }

    if (app.runInfo.has_value() && app.killRect.contains(tap.position)) {
      std::cout << "Killing " << app.description.name << std::endl;
      if (app.runInfo->paused) {
        kill(-app.runInfo->pid, SIGCONT);
      }
      kill(app.runInfo->pid, SIGINT);
      closeDialogs();
      break;
    }
  }
}

void
Launcher::doAction(const GestureConfig& config) {
  switch (config.action) {
    case GestureConfig::Launch: {
      auto app =
        std::find_if(apps.begin(), apps.end(), [&config](const auto& app) {
          return app.description.name == config.command;
        });
      if (app == apps.end()) {
        std::cerr << "App not found: " << config.command << std::endl;
        break;
      }

      switchApp(*app);

    } break;
    case GestureConfig::ShowApps: {
      drawAppsLauncher();
    } break;

    case GestureConfig::NextRunning: {
      auto it = std::find_if(apps.begin(), apps.end(), [this](auto& app) {
        return app.description.path == currentAppPath;
      });

      if (currentAppPath.empty() || it == apps.end()) {
        std::cerr << "No running app\n";
        break;
      }

      const auto start = it;
      it++;
      while (it != start) {
        if (it == apps.end()) {
          it = apps.begin();
        }

        if (it->runInfo.has_value()) {
          break;
        }

        it++;
      }

      switchApp(*it);
    } break;
    case GestureConfig::PrevRunning: {
      auto it = std::find_if(apps.rbegin(), apps.rend(), [this](auto& app) {
        return app.description.path == currentAppPath;
      });

      if (currentAppPath.empty() || it == apps.rend()) {
        std::cerr << "No running app\n";
        break;
      }

      const auto start = it;
      it++;
      while (it != start) {
        if (it == apps.rend()) {
          it = apps.rbegin();
        }
        if (it->runInfo.has_value()) {
          break;
        }
        it++;
      }

      switchApp(*it);
    } break;
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
Launcher::handleGesture(const Config& config, const Gesture& g) {
  printGesture(g);

  switch (state) {

    case State::ShowingLauncher:
      if constexpr (std::is_same_v<Gesture, TapGesture>) {
        handleLauncherTap(g);
        break;
      }
    case State::Default:
      // Default, look through config.
      for (const auto& gc : config.gestures) {
        if (gc.matches(g)) {
          doAction(gc);
        }
      }
      break;
  }
}

Launcher launcher;

void
cleanup(int signal) {
  pid_t childPid = 0;
  while ((childPid = waitpid(static_cast<pid_t>(-1), nullptr, WNOHANG)) > 0) {
    std::cout << "Exited: " << childPid << std::endl;

    auto app = std::find_if(
      launcher.apps.begin(), launcher.apps.end(), [childPid](auto& app) {
        return app.runInfo.has_value() && app.runInfo->pid == childPid;
      });

    if (app != launcher.apps.end()) {
      app->runInfo = std::nullopt;
    }
  }
}

std::atomic_bool shouldStop = false;

void
stop(int signal) {
  shouldStop = true;
}

int
Launcher::init() {
  auto deviceType = device::getDeviceType();
  if (!deviceType.has_value()) {
    std::cerr << "Error finding device type\n";
    return -1;
  }

  auto paths = device::getInputPaths(*deviceType);
  if (!inputMgr.open(paths.touchPath.data(), paths.touchTransform)
         .has_value()) {
    std::cerr << "Error opening inputMgr\n";
    return -1;
  }

  auto optConfig = readConfg();
  if (!optConfig.has_value()) {
    std::cerr << "Error reading config\n";
    return -1;
  }
  config = std::move(*optConfig);

  readApps();

  std::cout << "Apps:" << std::endl;
  for (const auto& app : apps) {
    std::cout << " - " << app.description.name << std::endl;
  }

  frameBuffer = fb::FrameBuffer::open();
  if (!frameBuffer.has_value()) {
    std::cerr << "Error opening framebuffer\n";
    return -1;
  }

  return 0;
}

void
Launcher::run() {
  while (!shouldStop) {
    auto events = inputMgr.waitForInput();

    if (auto* currentApp = getCurrentApp();
        currentApp != nullptr && !currentApp->runInfo.has_value()) {
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
        drawAppsLauncher();
      }
    }

    if (!events.has_value()) {
      std::cerr << "Error reading event\n";
      continue;
    }

    auto gestures = gestureController.handleEvents(*events);

    for (const auto& gesture : gestures) {
      std::visit([this](const auto& g) { handleGesture(config, g); }, gesture);
    }
  }
}

} // namespace

int
main(int argc, char* argv[]) {
  std::signal(SIGCHLD, cleanup);
  std::signal(SIGINT, stop);

  if (launcher.init() != 0) {
    return -1;
  }

  launcher.run();

  launcher.closeDialogs();

  return 0;
}
