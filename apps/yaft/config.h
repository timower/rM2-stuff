#pragma once

#include "Error.h"

#include <filesystem>
#include <optional>
#include <string>

#include "MathUtil.h"
#include "keymap.h"
#include "layout.h"

struct YaftConfig {

  // Layout for virtual keyboard.
  const Layout* layout = layouts.begin()->second;

  // Keymap of physical keyboard.
  const KeyMap* keymap = keymaps.begin()->second;

  bool autoRotate = true;
  rmlib::Rotation rotation = rmlib::Rotation::None;

  // Auto refresh full screen after 1024 updates.
  // Set to 0 to disable.
  int autoRefresh = 0;

  // Key repeat delay in ms
  int repeatDelay = 600;
  // Key repeat rate in chars / sec.
  int repeatRate = 25;

  static YaftConfig getDefault();
};

struct YaftConfigError {
  enum { Missing, Syntax } type;
  std::string msg;
};

struct YaftConfigAndError {
  YaftConfig config;

  std::optional<YaftConfigError> err;
};

std::filesystem::path
getConfigPath();

/// Load the config from the `~/.config/yaft/config.toml` location.
YaftConfigAndError
loadConfig(const std::filesystem::path& path);

OptError<>
saveDefaultConfig(const std::filesystem::path& path);

/// Always returns a config, either the default one or the one on the file
/// system. Will also make a new config file if it didn't exist.
///
/// If any error occured during the loading of the config, it's also returned.
YaftConfigAndError
loadConfigOrMakeDefault(const std::filesystem::path& path);
