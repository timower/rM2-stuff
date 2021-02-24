#include "Device.h"

#include <dirent.h>
#include <fstream>

namespace rmlib::device {

namespace {
constexpr auto screen_width = 1404;
constexpr auto screen_height = 1872;

constexpr auto rm1_touch_width = 767;
constexpr auto rm1_touch_height = 1023;

constexpr auto wacom_width = 15725;
constexpr auto wacom_height = 20967;

constexpr auto wacom_transform =
  Transform{ { { 0, float(screen_width) / wacom_width },
               { -float(screen_height) / wacom_height, 0 } },
             { 0, screen_height } };

const InputPaths rm1_paths = {
  // touch
  "/dev/input/event1",
  Transform{ { { -float(screen_width) / rm1_touch_width, 0 },
               { 0, -float(screen_height) / rm1_touch_height } },
             { screen_width, screen_height } },

  // pen
  "/dev/input/event0",
  wacom_transform,

  // keys
  "/dev/input/event2"
};

const InputPaths rm2_paths = {
  // touch
  "/dev/input/event2",
  Transform{ { { 1, 0 }, { 0, -1 } }, { 0, screen_height } },

  // pen
  "/dev/input/event1",
  wacom_transform,

  // keys
  "/dev/input/event0"
};
} // namespace

ErrorOr<DeviceType>
getDeviceType() {
  static const auto result = []() -> ErrorOr<DeviceType> {
    constexpr auto path = "/sys/devices/soc0/machine";
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      return Error{ "Couldn't open device path" };
    }

    std::string name;
    name.reserve(16);
    name.assign(std::istreambuf_iterator<char>(ifs),
                std::istreambuf_iterator<char>());

    if (name.find("2.0") == std::string::npos) {
      return DeviceType::reMarkable1;
    }
    return DeviceType::reMarkable2;
  }();

  return result;
}

const InputPaths&
getInputPaths(DeviceType type) {
  switch (type) {
    case DeviceType::reMarkable1:
      return rm1_paths;
    default:
    case DeviceType::reMarkable2:
      return rm2_paths;
  }
}

std::optional<Transform>
getInputTransform(std::string_view path) {
  auto devType = getDeviceType();
  if (devType.isError()) {
    return std::nullopt;
  }

  auto paths = getInputPaths(*devType);
  if (path == paths.touchPath) {
    return paths.touchTransform;
  }
  if (path == paths.penPath) {
    return paths.penTransform;
  }

  return std::nullopt;
}

std::vector<std::string>
listDirectory(std::string_view path, bool onlyFiles) {
  auto* dir = opendir(path.data());
  if (dir == nullptr) {
    return {};
  }

  std::vector<std::string> result;
  for (auto* dirent = readdir(dir); dirent != nullptr; dirent = readdir(dir)) {
    if (onlyFiles && dirent->d_type != DT_REG) {
      continue;
    }
    result.push_back(std::string(path) + "/" + std::string(dirent->d_name));
  }

  closedir(dir);

  return result;
}

} // namespace rmlib::device
