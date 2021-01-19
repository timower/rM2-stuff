#include "App.h"

#include <fstream>

std::optional<AppDescription>
AppDescription::read(std::string_view path) {
  std::ifstream ifs(path.data());
  if (!ifs.is_open()) {
    return std::nullopt;
  }

  AppDescription result;
  result.path = path;

  for (std::string line; std::getline(ifs, line);) {
    auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    auto key = line.substr(0, eq);
    auto value = line.substr(eq + 1);
    if (key == "name") {
      result.name = value;
    } else if (key == "desc") {
      result.description = value;
    } else if (key == "call") {
      result.command = value;
    } else if (key == "imgFile") {
      result.icon = value;
    }
  }

  if (result.name.empty()) {
    return std::nullopt;
  }

  return result;
}
