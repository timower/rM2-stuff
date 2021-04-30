#pragma once

#include <Canvas.h>
#include <FrameBuffer.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

struct AppRunInfo {
  pid_t pid;
  bool paused = false;

  // Indicates that the app should be removed when it exists
  bool shouldRemove = false;
};

struct AppDescription {
  std::string path; // path of app draft file, is unique.

  std::string name;
  std::string description;

  std::string command;
  std::string icon;

  std::optional<rmlib::ImageCanvas> iconImage;

  static std::optional<AppDescription> read(std::string_view path,
                                            std::string_view iconDir);
};

std::vector<AppDescription>
readAppFiles(std::string_view directory);

struct App {
  AppDescription description;

  std::optional<AppRunInfo> runInfo = std::nullopt;

  std::chrono::steady_clock::time_point lastActivated;

  std::optional<rmlib::MemoryCanvas> savedFb;

  // Used for UI:
  rmlib::Rect launchRect;
  rmlib::Rect killRect;

  App(AppDescription desc) : description(std::move(desc)) {}

  bool isRunning() const { return runInfo.has_value(); }
  bool isPaused() const { return isRunning() && runInfo->paused; }

  /// Starts a new instance of the app if it's not already running.
  /// \returns True if a new instance was started.
  bool launch();

  void stop();

  void pause(std::optional<rmlib::MemoryCanvas> screen = std::nullopt);
  void resume(rmlib::fb::FrameBuffer* fb = nullptr);
};
