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
# - auto: portrait, unless a type folio is connected.
# - portrait
# - landscape
orientation = "auto"

# Do a full refresh after 1024 updates.
# Set to 0 to disable auto refresh.
auto-refresh = 1024
)";

std::filesystem::path
getConfigPath() {
  const auto* home = getenv("HOME");
  if (home == nullptr || home[0] == 0) {
    home = "/home/root";
  }
  return std::filesystem::path(home) / ".config" / "yaft" / "config.toml";
}

template<typename Value>
ErrorOr<Value, YaftConfigError>
parseMapping(
  const toml::table& tbl,
  std::string_view name,
  Value defaultVal,
  std::initializer_list<std::pair<std::string_view, Value>> mapping) {
  auto entry = tbl[name].value<std::string_view>();
  if (!entry.has_value()) {
    return defaultVal;
  }

  auto it = std::find_if(mapping.begin(), mapping.end(), [&](const auto& pair) {
    return pair.first == *entry;
  });

  if (it == mapping.end()) {
    return YaftConfigError{
      YaftConfigError::Syntax,
      "Invalid " + std::string(name) + ": " + std::string(*entry),
    };
  }

  return it->second;
}

ErrorOr<YaftConfig, YaftConfigError>
getConfig(const toml::table& tbl) {
  YaftConfig cfg;

  cfg.layout = TRY(parseMapping(tbl, "layout", cfg.layout, layouts));
  cfg.keymap = TRY(parseMapping(tbl, "keymap", cfg.keymap, keymaps));

  cfg.orientation =
    TRY(parseMapping(tbl,
                     "orientation",
                     cfg.orientation,
                     { { "auto", YaftConfig::Orientation::Auto },
                       { "portrait", YaftConfig::Orientation::Protrait },
                       { "landscape", YaftConfig::Orientation::Landscape } }));

  cfg.autoRefresh = tbl["auto-refresh"].value_or(cfg.autoRefresh);

  return cfg;
}

} // namespace

YaftConfig
YaftConfig::getDefault() {
  auto tbl = toml::parse(default_config);
  return *getConfig(tbl);
}

ErrorOr<YaftConfig, YaftConfigError>
loadConfig() {
  toml::table tbl;

  const auto path = getConfigPath();
  if (!std::filesystem::exists(path)) {
    return YaftConfigError{ YaftConfigError::Missing, path.string() };
  }

  try {
    tbl = toml::parse_file(getConfigPath().c_str());
  } catch (const toml::parse_error& err) {
    return YaftConfigError{ YaftConfigError::Syntax,
                            std::to_string(err.source().begin.line) + ": " +
                              std::string(err.description()) };
  }

  return getConfig(tbl);
}

OptError<>
saveDefaultConfig() {
  const auto path = getConfigPath();
  const auto dir = path.parent_path();

  if (!std::filesystem::is_directory(dir)) {
    std::filesystem::create_directories(dir);
  }

  std::ofstream ofs(path);
  ofs << default_config;

  return NoError{};
}
