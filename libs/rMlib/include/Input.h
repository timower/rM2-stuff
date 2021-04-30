#pragma once

#include "Error.h"
#include "MathUtil.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

struct libevdev;
struct udev;
struct udev_monitor;

namespace rmlib::input {
constexpr static auto max_num_slots = 32;

struct TouchEvent {
  enum { Down, Up, Move } type;
  int id;
  int slot;

  Point location;
  int pressure;

  constexpr bool isDown() const { return type == Down; }
  constexpr bool isUp() const { return type == Up; }
  constexpr bool isMove() const { return type == Move; }
};

struct PenEvent {
  enum { TouchDown, TouchUp, ToolClose, ToolLeave, Move } type;

  Point location;
  int distance;
  int pressure;

  int id = -1234;

  constexpr bool isDown() const { return type == TouchDown; }
  constexpr bool isUp() const { return type == TouchUp; }
  constexpr bool isMove() const { return type == Move; }
};

struct KeyEvent {
  enum { Release = 0, Press = 1, Repeat = 2 } type;
  int keyCode;
};

template<typename T>
constexpr bool is_pointer_event = !std::is_same_v<std::decay_t<T>, KeyEvent>;

struct SwipeGesture {
  enum Direction { Up, Down, Left, Right };

  Direction direction;
  Point startPosition;
  Point endPosition;
  int fingers;
};

struct PinchGesture {
  enum Direction { In, Out };
  Direction direction;
  Point position;
  int fingers;
};

struct TapGesture {
  int fingers;
  Point position;
};

using Gesture = std::variant<SwipeGesture, PinchGesture, TapGesture>;

using Event = std::variant<TouchEvent, PenEvent, KeyEvent>;

struct InputDeviceBase {
  int fd;
  libevdev* evdev;
  std::string path;

  void grab();
  void ungrab();
  virtual void flood() = 0;

  virtual ~InputDeviceBase();

  virtual OptError<> readEvents(std::vector<Event>& out) = 0;

protected:
  InputDeviceBase(int fd, libevdev* evdev, std::string path)
    : fd(fd), evdev(evdev), path(std::move(path)) {}
};

struct BaseDevices {
  InputDeviceBase& pen;
  InputDeviceBase& touch;
  InputDeviceBase& key;
};

struct InputManager {
  ErrorOr<InputDeviceBase*> open(std::string_view input,
                                 Transform inputTransform);

  ErrorOr<InputDeviceBase*> open(std::string_view input);

  /// Opens all devices for the current device type.
  /// \param monitor If true monitor for new devices and automatically add them.
  ///                Will also remove devices when unplugged.
  ErrorOr<BaseDevices> openAll(bool monitor = true);

  InputManager();
  ~InputManager();

  InputManager(InputManager&& other) : devices(std::move(other.devices)) {
    other.devices.clear();
  }

  InputManager& operator=(InputManager&& other) {
    devices.clear();
    std::swap(other, *this);
    return *this;
  }

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  ErrorOr<std::vector<Event>> waitForInput(
    fd_set& fdSet,
    int maxFd,
    std::optional<std::chrono::microseconds> timeout = std::nullopt);

  template<typename... ExtraFds>
  auto waitForInput(std::optional<std::chrono::microseconds> timeout,
                    ExtraFds... extraFds)
    -> ErrorOr<std::conditional_t<
      sizeof...(ExtraFds) == 0,
      std::vector<Event>,
      std::pair<std::vector<Event>, std::array<bool, sizeof...(ExtraFds)>>>> {
    static_assert((std::is_same_v<ExtraFds, int> && ...));

    fd_set fds;
    FD_ZERO(&fds);
    if constexpr (sizeof...(ExtraFds) > 0) {
      constexpr auto fd_set = [](auto fd, auto& fds) { FD_SET(fd, &fds); };
      (fd_set(extraFds, fds), ...);
    }

    auto maxFd = std::max({ 0, extraFds... });

    auto res = TRY(waitForInput(fds, maxFd, timeout));
    if constexpr (sizeof...(ExtraFds) == 0) {
      return res;
    } else {
      std::array<bool, sizeof...(extraFds)> extraResult;
      int i = 0;

      constexpr auto fd_isset = [](auto fd, auto& fds) {
        return FD_ISSET(fd, &fds);
      };
      ((extraResult[i++] = fd_isset(extraFds, fds)), ...);

      return std::pair{ res, extraResult };
    }
  }

  std::optional<BaseDevices> getBaseDevices() const { return baseDevices; }

  /// members
  std::unordered_map<std::string_view, std::unique_ptr<InputDeviceBase>>
    devices;

private:
  std::optional<BaseDevices> baseDevices;
  udev* udevHandle = nullptr;
  udev_monitor* udevMonitor = nullptr;
  int udevMonitorFd = -1;
};

struct GestureController {
  // pixels to move before detecting swipe or pinch
  constexpr static int start_threshold = 50;
  constexpr static auto tap_time = std::chrono::milliseconds(150);

  struct SlotState {
    bool active = false;
    Point currentPos;
    Point startPos;
    std::chrono::steady_clock::time_point time;
  };

  Gesture getGesture(Point currentDelta);
  void handleTouchDown(const TouchEvent& event);
  void handleTouchMove(const TouchEvent& event);
  std::optional<Gesture> handleTouchUp(const TouchEvent& event);

  std::pair<std::vector<Gesture>, std::vector<Event>> handleEvents(
    const std::vector<Event>& events);

  void sync(InputDeviceBase& device);

  void reset() {
    started = false;
    tapFingers = 0;
  }

  int getCurrentFingers() {
    return std::count_if(
      slots.begin(), slots.end(), [](const auto& slot) { return slot.active; });
  }

  // members
  int tapFingers = 0;

  std::array<SlotState, max_num_slots> slots;

  bool started = false;
  Gesture gesture;
};

} // namespace rmlib::input
