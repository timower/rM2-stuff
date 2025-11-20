#include "config.h"
#include "Error.h"

#include <algorithm>
#include <filesystem>
#include <toml++/toml.h>

namespace {
constexpr std::string_view default_config = R"(
# Layout of the virtual keyboard.
layout = "qwerty"

# Keymap for any physical keyboards. Defaults to the us rM pogo keyboard.
keymap = "rm-qwerty"

# Orientation of yaft:
# Auto rotate if keyboard is connected
auto-rotate = true
# Orientation if no keyboard is connected
rotation = "none"

# Do a full refresh after 1024 updates.
# Set to 0 to disable auto refresh.
auto-refresh = 1024

# Repeat delay for keyboards.
repeat-delay = 600
# Repeat rate in chars per second.
repeat-rate = 25
)";

std::filesystem::path
getConfigPath() {
  const auto configDir = [] {
    if (const auto* xdgCfg = getenv("XDG_CONFIG_HOME");
        xdgCfg != nullptr && xdgCfg[0] != 0) {
      return std::filesystem::path(xdgCfg);
    }

    const auto* home = getenv("HOME");
    if (home == nullptr || home[0] == 0) {
      home = "/home/root";
    }

    return std::filesystem::path(home) / ".config" / "yaft";
  }();

  return configDir / "config.toml";
}

void
addError(std::optional<YaftConfigError>& error, YaftConfigError err) {
  if (!error.has_value()) {
    error = err;
    return;
  }
  error->msg += "\r\n" + err.msg;
}

template<typename Value>
void
parseValue(std::optional<YaftConfigError>& error,
           Value& value,
           toml::table& tbl,
           std::string_view name) {
  value = tbl[name].value_or(value);
  tbl.erase(name);
}

template<typename Value>
void
parseMapping(
  std::optional<YaftConfigError>& error,
  Value& value,
  toml::table& tbl,
  std::string_view name,
  std::initializer_list<std::pair<std::string_view, Value>> mapping) {

  auto entry = tbl[name].value<std::string_view>();
  if (!entry.has_value()) {
    return;
  }
  tbl.erase(name);

  auto it = std::find_if(mapping.begin(), mapping.end(), [&](const auto& pair) {
    return pair.first == *entry;
  });

  if (it == mapping.end()) {
    addError(
      error,
      {
        YaftConfigError::Syntax,
        "error: Invalid " + std::string(name) + ": " + std::string(*entry),
      });
    return;
  }

  value = it->second;
}

YaftConfigAndError
getConfig(const toml::table& input) {
  auto tbl = input;

  std::optional<YaftConfigError> errors;
  YaftConfig cfg;

  parseMapping(errors, cfg.layout, tbl, "layout", layouts);
  parseMapping(errors, cfg.keymap, tbl, "keymap", keymaps);
  parseMapping(errors,
               cfg.rotation,
               tbl,
               "rotation",
               { { "none", rmlib::Rotation::None },
                 { "clockwise", rmlib::Rotation::Clockwise },
                 { "inverted", rmlib::Rotation::Inverted },
                 { "counterclockwise", rmlib::Rotation::CounterClockwise } });

  parseValue(errors, cfg.autoRotate, tbl, "auto-rotate");
  parseValue(errors, cfg.autoRefresh, tbl, "auto-refresh");
  parseValue(errors, cfg.repeatDelay, tbl, "repeat-delay");
  parseValue(errors, cfg.repeatRate, tbl, "repeat-rate");

  if (!tbl.empty()) {
    for (const auto& [key, _] : tbl) {
      addError(errors,
               { YaftConfigError::Syntax,
                 "warning: Unknown key: " + std::string(key) });
    }
  }

  return { cfg, errors };
}

} // namespace

YaftConfig
YaftConfig::getDefault() {
  auto tbl = toml::parse(default_config);
  return getConfig(tbl).config;
}

YaftConfigAndError
loadConfig() {
  toml::table tbl;

  const auto path = getConfigPath();
  if (!std::filesystem::exists(path)) {
    return { YaftConfig::getDefault(),
             YaftConfigError{ YaftConfigError::Missing, path.string() } };
  }

  try {
    tbl = toml::parse_file(getConfigPath().c_str());
  } catch (const toml::parse_error& err) {
    return { YaftConfig::getDefault(),
             YaftConfigError{ YaftConfigError::Syntax,
                              std::to_string(err.source().begin.line) + ": " +
                                std::string(err.description()) } };
  }

  return getConfig(tbl);
}

OptError<>
saveDefaultConfig() {
  const auto path = getConfigPath();
  const auto dir = path.parent_path();

  std::error_code ec;

  if (!std::filesystem::is_directory(dir)) {
    std::filesystem::create_directories(dir, ec);
  }

  if (ec) {
    return Error::make(ec.message());
  }

  std::ofstream ofs(path);
  ofs << default_config;

  if (ofs.fail()) {
    return Error::make("Error writing config");
  }

  return {};
}

YaftConfigAndError
loadConfigOrMakeDefault() {
  auto result = loadConfig();
  if (!result.err.has_value()) {
    return result;
  }

  auto& err = *result.err;
  if (err.type == YaftConfigError::Missing) {
    err.msg = "No config, creating new one\r\n";
    if (const auto optErr = saveDefaultConfig(); !optErr.has_value()) {
      err.msg += optErr.error().msg + "\r\n";
    }
  } else {
    err.msg = "Config syntax issues:\r\n" + err.msg;
  }

  return result;
}
