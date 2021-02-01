#pragma once

#include "MathUtil.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

struct libevdev;

namespace rmlib::input {
constexpr static auto max_num_slots = 32;

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
    libevdev* dev;
    Transform transform;

    int slot = 0;
    std::array<Event, max_num_slots> slots;

    std::unordered_set<int> changedSlots;
    std::vector<Event> events;

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
  }

  InputManager& operator=(InputManager&& other) {
    closeAll();
    std::swap(other, *this);
    return *this;
  }

  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  std::optional<std::vector<Event>> waitForInput(
    fd_set& fdSet,
    int maxFd,
    std::optional<std::chrono::microseconds> timeout = std::nullopt);

  template<typename... ExtraFds>
  auto waitForInput(std::optional<std::chrono::microseconds> timeout,
                    ExtraFds... extraFds)
    -> std::optional<std::conditional_t<
      sizeof...(ExtraFds) == 0,
      std::vector<Event>,
      std::pair<std::vector<Event>, std::array<bool, sizeof...(ExtraFds)>>>> {
    static_assert((std::is_same_v<ExtraFds, int> && ...));

    fd_set fds;
    FD_ZERO(&fds);
    (FD_SET(extraFds, &fds), ...);

    auto maxFd = std::max({ 0, extraFds... });

    auto res = waitForInput(fds, maxFd, timeout);
    if constexpr (sizeof...(ExtraFds) == 0) {
      return res;
    } else {

      if (!res.has_value()) {
        return std::nullopt;
      }

      std::array<bool, sizeof...(extraFds)> extraResult;
      int i = 0;
      ((extraResult[i++] = FD_ISSET(extraFds, &fds)), ...);

      return std::pair{ *res, extraResult };
    }
  }

  std::vector<Event> readEvents(int);
  static std::vector<Event> readEvents(InputDevice& device);

  void grab();
  void ungrab();
  void flood();

  /// members
  std::unordered_map<int, InputDevice> devices;
};

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

  std::vector<Gesture> handleEvents(const std::vector<Event>& events);

  void sync(InputManager::InputDevice& device);

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
