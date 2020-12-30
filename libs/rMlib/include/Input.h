#pragma once

#include "MathUtil.h"

#include <optional>
#include <unordered_map>
#include <variant>

namespace rmlib::input {
struct TouchEvent {
  enum { Down, Up, Move } type;
  int id;
  int slot;

  Point location;
  int pressure;
};

// TODO
struct PenEvent {
  enum { TouchDown, TouchUp, ToolClose, ToolLeave, Move } type;
  Point location;
  int distance;
  int pressure;
};

struct KeyEvent {
  enum { Release = 0, Press = 1, Repeat = 2 } type;
  int keyCode;
};

using Event = std::variant<TouchEvent, PenEvent, KeyEvent>;

struct InputManager {
  struct InputDevice {
    int fd;
    Transform transform;

    int slot = 0;
    Event slots[10];

    template<typename T>
    T& getSlot() {
      if (!std::holds_alternative<T>(slots[slot])) {
        slots[slot] = T{};
      }
      return std::get<T>(slots[slot]);
    }
  };

  std::optional<int> open(const char* input,
                          Transform inputTransform = Transform::identity());

  /// Opens all devices for the current device type.
  bool openAll();

  void close(int fd);
  void closeAll();

  InputManager() = default;
  ~InputManager() { closeAll(); }

  InputManager(InputManager&& other) : devices(std::move(other.devices)) {
    other.devices.clear();
    other.maxFd = 0;
  }

  InputManager& operator=(InputManager&& other) {
    closeAll();
    std::swap(other, *this);
    return *this;
  }

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  std::optional<Event> waitForInput(double timeout = 0);
  static std::optional<Event> readEvent(InputDevice& device);

  int maxFd = 0;
  std::unordered_map<int, InputDevice> devices;
};

} // namespace rmlib::input
