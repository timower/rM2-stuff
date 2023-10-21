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

#if defined(EMULATE) && !defined(EMULATE_UINPUT)
#include "../EmulatedKeyCodes.h"
#else
#include <linux/input-event-codes.h>
#endif

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
  InputDeviceBase* pen = nullptr;
  InputDeviceBase* touch = nullptr;
  InputDeviceBase* key = nullptr;
};

struct InputManager {
  ErrorOr<InputDeviceBase*> open(std::string_view input,
                                 Transform inputTransform);

  ErrorOr<InputDeviceBase*> open(std::string_view input);

  /// Opens all devices for the current device type.
  /// \param monitor If true monitor for new devices and automatically add them.
  ///                Will also remove devices when unplugged.
  ErrorOr<BaseDevices> openAll(bool monitor = true);

  InputManager() = default;
  ~InputManager() = default;

  InputManager(InputManager&& other) = default;
  InputManager& operator=(InputManager&& other) = default;

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
  unistdpp::FD udevMonitorFd;

  template<typename T>
  struct UdevDeleter {
    void operator()(T* t);
  };

  template<typename T>
  using UniquePtr = std::unique_ptr<T, UdevDeleter<T>>;

  UniquePtr<udev> udevHandle;
  UniquePtr<udev_monitor> udevMonitor;
};

} // namespace rmlib::input
