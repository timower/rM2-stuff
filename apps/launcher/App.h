#pragma once

#include <Canvas.h>

#include <chrono>
#include <optional>
#include <string>

struct AppRunInfo {
  pid_t pid;
  bool paused = false;
};

struct AppDescription {
  std::string path; // path of app draft file, is unique.

  std::string name;
  std::string description;

  std::string command;
  std::string icon;

  static std::optional<AppDescription> read(std::string_view path);
};

struct App {
  AppDescription description;
  std::optional<AppRunInfo> runInfo = std::nullopt;

  std::chrono::steady_clock::time_point lastActivated;

  std::optional<rmlib::MemoryCanvas> savedFb;

  // Used for UI:
  rmlib::Rect launchRect;
  rmlib::Rect killRect;

  App(AppDescription desc) : description(std::move(desc)) {}

  std::string getDisplayName() const {
    auto result = description.name;

    if (runInfo.has_value()) {
      if (runInfo->paused) {
        result = "*" + result;
      } else {
        result = ">" + result;
      }
    }

    return result;
  }
};
