#pragma once

#include "Error.h"
#include "MathUtil.h"

#include <optional>
#include <string_view>
#include <vector>

/// Contains any device specific information.
namespace rmlib::device {

enum class DeviceType { reMarkable1, reMarkable2 };

/// \returns The device type on which we're currently running or nullopt if
/// detection fails.
ErrorOr<DeviceType>
getDeviceType();

struct InputPaths {
  std::string_view touchPath;
  Transform touchTransform;

  std::string_view penPath;
  Transform penTransform;

  std::string_view buttonPath;
};

const InputPaths&
getInputPaths(DeviceType type);

std::optional<Transform>
getInputTransform(std::string_view path);

// TODO: battery paths

std::vector<std::string>
listDirectory(std::string_view path, bool onlyFiles = true);

} // namespace rmlib::device
