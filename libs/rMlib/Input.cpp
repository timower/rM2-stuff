#include "Input.h"

#include "Device.h"

#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>

#include <iostream>

namespace rmlib::input {

std::optional<int>
InputManager::open(const char* input, Transform inputTransform) {
  int fd = ::open(input, O_RDONLY);
  if (fd < 0) {
    return std::nullopt;
  }

  devices.emplace(fd, InputDevice{ fd, std::move(inputTransform) });
  maxFd = std::max(maxFd, fd + 1);
  return fd;
}

bool
InputManager::openAll() {
  auto type = device::getDeviceType();
  if (!type.has_value()) {
    return false;
  }

  auto paths = device::getInputPaths(*type);

  if (!open(paths.touchPath.data(), paths.touchTransform)) {
    return false;
  }

  if (!open(paths.penPath.data(), paths.penTransform)) {
    return false;
  }

  if (!open(paths.buttonPath.data())) {
    return false;
  }

  return true;
}

void
InputManager::close(int fd) {
  auto it = devices.find(fd);
  if (it == devices.end()) {
    return;
  }
  devices.erase(it);
  maxFd = devices.empty() ? 0
                          : std::max_element(
                              devices.begin(),
                              devices.end(),
                              [](const auto& deva, const auto& devb) {
                                return deva.first < devb.first;
                              })->first +
                              1;
}

void
InputManager::closeAll() {
  for (const auto& [fd, _] : devices) {
    (void)_;
    ::close(fd);
  }
  devices.clear();
  maxFd = 0;
}

std::optional<Event>
InputManager::waitForInput(double timeout) {
  if (devices.empty()) {
    return std::nullopt;
  }

  fd_set fds;
  FD_ZERO(&fds);
  for (auto& [fd, _] : devices) {
    (void)_;
    FD_SET(fd, &fds);
  }

  timeval tv;
  tv.tv_sec = timeout;
  tv.tv_usec = (timeout - tv.tv_sec) * 1000000.0;

  auto ret =
    select(maxFd, &fds, nullptr, nullptr, timeout == 0.0 ? nullptr : &tv);
  if (ret < 0) {
    perror("Select on input failed");
    return std::nullopt;
  }

  if (ret == 0) {
    // timeout
    return std::nullopt;
  }

  // Return the first event we see.
  for (auto& [fd, device] : devices) {
    if (!FD_ISSET(fd, &fds)) {
      continue;
    }

    return readEvent(device);
  }

  return std::nullopt;
}

std::optional<Event>
InputManager::readEvent(InputDevice& device) {
  bool setType = false;

  // Read until SYN.
  while (true) {
    input_event event;
    auto size = read(device.fd, &event, sizeof(input_event));
    if (size != sizeof(input_event)) {
      perror("Error reading input");
      return std::nullopt;
    }

    if (event.type == EV_SYN && event.code == SYN_REPORT) {
      break;
    }

    if (event.type == EV_ABS) {
      if (event.code == ABS_MT_SLOT) {
        device.slot = event.value;
        device.getSlot<TouchEvent>().slot = event.value;
      } else if (event.code == ABS_MT_TRACKING_ID) {
        auto& slot = device.getSlot<TouchEvent>();
        if (event.value == -1) {
          slot.type = TouchEvent::Up;
          setType = true;
        } else if (event.value != slot.id) {
          slot.type = TouchEvent::Down;
          slot.id = event.value;
          setType = true;
        }
      } else if (event.code == ABS_MT_POSITION_X) {
        auto& slot = device.getSlot<TouchEvent>();
        slot.location.x = event.value;
      } else if (event.code == ABS_MT_POSITION_Y) {
        auto& slot = device.getSlot<TouchEvent>();
        slot.location.y = event.value;
      } else if (event.code == ABS_X) {
        device.getSlot<PenEvent>().location.x = event.value;
      } else if (event.code == ABS_Y) {
        device.getSlot<PenEvent>().location.y = event.value;
      } else if (event.code == ABS_DISTANCE) {
        device.getSlot<PenEvent>().distance = event.value;
      } else if (event.code == ABS_PRESSURE) {
        device.getSlot<PenEvent>().pressure = event.value;
      }
    }

    if (event.type == EV_KEY) {
      if (event.code == BTN_TOOL_PEN) {
        if (event.value == KeyEvent::Press) {
          device.getSlot<PenEvent>().type = PenEvent::ToolClose;
          setType = true;
        } else {
          device.getSlot<PenEvent>().type = PenEvent::ToolLeave;
          setType = true;
        }
      } else if (event.code == BTN_TOUCH) {
        std::cout << "TOUCH\n";
        if (event.value == KeyEvent::Press) {
          device.getSlot<PenEvent>().type = PenEvent::TouchDown;
          setType = true;
        } else {
          device.getSlot<PenEvent>().type = PenEvent::TouchUp;
          setType = true;
        }
      } else {
        auto& slot = device.getSlot<KeyEvent>();
        slot.type = static_cast<decltype(slot.type)>(event.value);
        slot.keyCode = event.code;
      }
    }
  }

  auto result = device.slots[device.slot];

  // Set type to move if we didn't set it to up or down.
  if (!setType) {
    std::visit(
      [](auto& r) {
        using Type = std::decay_t<decltype(r)>;
        if constexpr (!std::is_same_v<Type, KeyEvent>) {
          r.type = Type::Move;
        }
      },
      result);
  }

  // Transform result if needed
  std::visit(
    [&device](auto& r) {
      using Type = std::decay_t<decltype(r)>;
      if constexpr (!std::is_same_v<Type, KeyEvent>) {
        r.location = device.transform * r.location;
      }
    },
    result);

  return result;
}
} // namespace rmlib::input
