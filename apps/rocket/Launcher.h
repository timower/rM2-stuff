#pragma once

#include "App.h"
#include "CommandSocket.h"
#include "Config.h"

#include <Canvas.h>
#include <FrameBuffer.h>
#include <Input.h>

#include <atomic>
#include <optional>
#include <string>
#include <vector>

enum class State { Default, ShowingLauncher };

struct Launcher {
  Launcher();

  OptError<> init();
  void run();

  void readApps();
  App* getCurrentApp();

  void drawAppsLauncher();
  void closeLauncher();

  void switchApp(App& app);

  void handleLauncherTap(rmlib::input::TapGesture tap);

  void doAction(const ActionConfig& config);

  template<typename Gesture>
  void handleGesture(const Gesture& g);

  void handleChildStop(pid_t pid) { stoppedApps.push_back(pid); };

  App* getApp(std::string_view name) {
    auto app = std::find_if(apps.begin(), apps.end(), [&name](const auto& app) {
      return app.description.name == name;
    });

    if (app == apps.end()) {
      return nullptr;
    }
    return &*app;
  }

  // Members
  std::vector<App> apps;
  std::string currentAppPath;
  State state = State::Default;

  std::vector<pid_t> stoppedApps;

  std::optional<rmlib::fb::FrameBuffer> frameBuffer;
  rmlib::input::InputManager inputMgr;

  std::optional<std::pair<rmlib::Point, rmlib::MemoryCanvas>> backupBuffer;

  std::optional<rmlib::input::FileDescriptors> inputFds;
  rmlib::input::GestureController gestureController;

  Config config;

  CommandSocket cmdSocket;

  std::atomic_bool shouldStop = false;
};
