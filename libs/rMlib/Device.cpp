#include "Device.h"

#include <unistdpp/file.h>

#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>

namespace rmlib::device {

namespace {
constexpr auto screen_width = 1404;
constexpr auto screen_height = 1872;

constexpr auto rm1_touch_width = 767;
constexpr auto rm1_touch_height = 1023;

constexpr auto wacom_width = 15725;
constexpr auto wacom_height = 20967;

constexpr auto wacom_transform =
  Transform{ { { { 0, float(screen_width) / wacom_width },
                 { -float(screen_height) / wacom_height, 0 } } },
             { 0, screen_height } };

struct BaseDeviceName : BaseDevice {
  std::string_view name;
};

using BaseDevices = std::array<BaseDeviceName, 3>;

const BaseDevices rm1_paths = { {
  // touch
  { { InputType::MultiTouch,
      Transform{ { { { -float(screen_width) / rm1_touch_width, 0 },
                     { 0, -float(screen_height) / rm1_touch_height } } },
                 { screen_width, screen_height } } },
    "cyttsp5_mt" },

  // pen
  { { InputType::Pen, wacom_transform }, "Wacom I2C Digitizer" },

  // keys
  { { InputType::Key, {} }, "gpio-keys" },
} };

const BaseDevices rm2_paths = { {
  // touch
  { { InputType::MultiTouch,
      Transform{ { { { 1, 0 }, { 0, -1 } } }, { 0, screen_height } } },
    "pt_mt" },

  // pen
  { { InputType::Pen, wacom_transform }, "Wacom I2C Digitizer" },

  // keys
  { { InputType::Key, {} }, "snvs-powerkey" },
} };

const BaseDevices&
getInputNames(DeviceType type) {
  switch (type) {
    case DeviceType::reMarkable1:
      return rm1_paths;
    default:
    case DeviceType::reMarkable2:
      return rm2_paths;
  }
}
} // namespace

ErrorOr<DeviceType>
getDeviceType() {
#ifdef EMULATE
  return DeviceType::reMarkable2;
#else
  static const auto result = []() -> ErrorOr<DeviceType> {
    constexpr auto path = "/sys/devices/soc0/machine";
    auto name = TRY(unistdpp::readFile(path));
    if (name.find("2.0") == std::string::npos) {
      return DeviceType::reMarkable1;
    }
    return DeviceType::reMarkable2;
  }();

  return result;
#endif
}

std::optional<BaseDevice>
getBaseDevice(std::string_view name) {
  return getDeviceType()
    .transform([&](auto devType) -> std::optional<BaseDevice> {
      for (const auto& device : getInputNames(devType)) {
        if (name.find(device.name) != std::string_view::npos) {
          return device;
        }
      }
      return std::nullopt;
    })
    .value_or(std::nullopt);
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

bool
isPogoConnected() {
#ifndef EMULATE
  constexpr auto path = "/sys/pogo/status/pogo_connected";
#else
  constexpr auto path = "/tmp/pogo";
#endif

  return unistdpp::open(path, O_RDWR)
    .and_then([](unistdpp::FD fd) {
      return fd.readAll<char>().transform([&](char buf) { return buf == '1'; });
    })
    .value_or(false);
}
} // namespace rmlib::device
