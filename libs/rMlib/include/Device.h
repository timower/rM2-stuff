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

enum class InputType { MultiTouch, Pen, Key };
struct BaseDevice {
  InputType type;
  Transform transform;
};

std::optional<BaseDevice>
getBaseDevice(std::string_view name);

// TODO: battery paths

std::vector<std::string>
listDirectory(std::string_view path, bool onlyFiles = true);

bool
IsPogoConnected();

} // namespace rmlib::device
