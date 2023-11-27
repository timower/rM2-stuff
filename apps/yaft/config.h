#pragma once

#include "Error.h"

#include <optional>
#include <string>

#include "keymap.h"
#include "layout.h"

struct YaftConfig {
  enum class Orientation { Auto, Protrait, Landscape };

  // Layout for virtual keyboard.
  const Layout* layout = nullptr;

  // Keymap of physical keyboard.
  const KeyMap* keymap = nullptr;

  Orientation orientation = Orientation::Auto;

  // Auto refresh full screen after 1024 updates.
  // Set to 0 to disable.
  int autoRefresh = 0;

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

/// Load the config from the `~/.config/yaft/config.toml` location.
ErrorOr<YaftConfig, YaftConfigError>
loadConfig();

OptError<>
saveDefaultConfig();

/// Always returns a config, either the default one or the one on the file
/// system. Will also make a new config file if it didn't exist.
///
/// If any error occured during the loading of the config, it's also returned.
YaftConfigAndError
loadConfigOrMakeDefault();
