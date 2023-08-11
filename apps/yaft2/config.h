#pragma once

#include "Error.h"
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

/// Load the config from the `~/.config/yaft/config.toml` location.
ErrorOr<YaftConfig, YaftConfigError>
loadConfig();

OptError<>
saveDefaultConfig();
