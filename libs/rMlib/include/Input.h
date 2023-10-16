#pragma once

#include "Error.h"
#include "MathUtil.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <unistdpp/poll.h>
#include <unistdpp/unistdpp.h>

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
  unistdpp::FD fd;
  libevdev* evdev;
  std::string path;

  void grab();
  void ungrab();
  virtual void flood() = 0;

  virtual ~InputDeviceBase();

  virtual OptError<> readEvents(std::vector<Event>& out) = 0;

protected:
  InputDeviceBase(unistdpp::FD fd, libevdev* evdev, std::string path)
    : fd(std::move(fd)), evdev(evdev), path(std::move(path)) {}
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

  // TODO: rule of 0 this
  InputManager(InputManager&& other)
    : devices(std::move(other.devices))
    , baseDevices(other.baseDevices)
    , udevHandle(other.udevHandle)
    , udevMonitor(other.udevMonitor)
    , udevMonitorFd(std::move(other.udevMonitorFd)) {
    other.devices.clear();
    other.baseDevices = std::nullopt;
    other.udevHandle = nullptr;
    other.udevMonitor = nullptr;
  }

  InputManager& operator=(InputManager&& other) {
    devices.clear();
    baseDevices = std::nullopt;
    udevHandle = nullptr;
    udevMonitor = nullptr;
    udevMonitorFd.close();

    std::swap(other, *this);
    return *this;
  }

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  ErrorOr<std::vector<Event>> waitForInput(
    std::vector<pollfd>& extraFds,
    std::optional<std::chrono::milliseconds> timeout = std::nullopt);

  template<typename... ExtraFds>
  auto waitForInput(std::optional<std::chrono::milliseconds> timeout,
                    const ExtraFds&... extraFds)
    -> ErrorOr<std::conditional_t<
      sizeof...(ExtraFds) == 0,
      std::vector<Event>,
      std::pair<std::vector<Event>, std::array<bool, sizeof...(ExtraFds)>>>> {

    static_assert(((std::is_same_v<ExtraFds, unistdpp::FD> ||
                    std::is_same_v<ExtraFds, int>)&&...));

    std::vector<pollfd> fds;

    if constexpr (sizeof...(ExtraFds) > 0) {
      fds.reserve(sizeof...(ExtraFds));
      constexpr auto fd_set = [](const auto& fd, auto& fds) {
        if constexpr (std::is_same_v<decltype(fd), const int&>) {
          fds.emplace_back(pollfd{ .fd = fd, .events = POLLIN, .revents = 0 });
        } else {
          fds.emplace_back(unistdpp::waitFor(fd, unistdpp::Wait::READ));
        }
      };
      (fd_set(extraFds, fds), ...);
    }

    auto res = TRY(waitForInput(fds, timeout));
    if constexpr (sizeof...(ExtraFds) == 0) {
      return res;
    } else {
      std::array<bool, sizeof...(extraFds)> extraResult;
      for (std::size_t i = 0; i < sizeof...(extraFds); i++) {
        extraResult[i] = unistdpp::canRead(fds[i]);
      }

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
  unistdpp::FD udevMonitorFd;
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
