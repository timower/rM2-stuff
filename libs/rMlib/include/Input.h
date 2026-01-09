#pragma once

#include "Error.h"
#include "MathUtil.h"

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>
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
  enum { Down, Up, Move } type = Move;
  int id{};
  int slot{};

  Point location;
  int pressure{};

  constexpr bool isDown() const { return type == Down; }
  constexpr bool isUp() const { return type == Up; }
  constexpr bool isMove() const { return type == Move; }
};

struct PenEvent {
  constexpr static auto pen_id = -1234;
  enum { TouchDown, TouchUp, ToolClose, ToolLeave, Move } type = Move;

  Point location = {};
  int distance = 0;
  int pressure = 0;

  int id = pen_id;

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

using Event = std::variant<TouchEvent, PenEvent, KeyEvent>;

struct InputDeviceBase {
  struct EvDevDeleter {
    void operator()(libevdev* evdev);
  };
  using EvDevPtr = std::unique_ptr<libevdev, EvDevDeleter>;

  unistdpp::FD fd;
  EvDevPtr evdev;

  std::string path;

  const char* getName() const;

  void grab() const;
  void ungrab() const;
  virtual void flood() = 0;

  virtual ~InputDeviceBase() = default;

  virtual OptError<> readEvents(std::vector<Event>& out) = 0;

protected:
  InputDeviceBase(unistdpp::FD fd, EvDevPtr evdev, std::string path)
    : fd(std::move(fd)), evdev(std::move(evdev)), path(std::move(path)) {}
};

struct BaseDevices {
  InputDeviceBase* pen = nullptr;
  InputDeviceBase* touch = nullptr;
  InputDeviceBase* key = nullptr;

  InputDeviceBase* pogoKeyboard = nullptr;
};

struct InputManager {
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
                    std::is_same_v<ExtraFds, int>) &&
                   ...));

    std::vector<pollfd> fds;

    if constexpr (sizeof...(ExtraFds) > 0) {
      fds.reserve(sizeof...(ExtraFds));
      constexpr auto fd_set = [](const auto& fd, auto& fds) {
        if constexpr (std::is_same_v<decltype(fd), const int&>) {
          fds.emplace_back(pollfd{ .fd = fd, .events = POLLIN, .revents = 0 });
        } else {
          fds.emplace_back(unistdpp::waitFor(fd, unistdpp::Wait::Read));
        }
      };
      (fd_set(extraFds, fds), ...);
    }

    auto res = TRY(waitForInput(fds, timeout));
    if constexpr (sizeof...(ExtraFds) == 0) {
      return res;
    } else {
      std::array<bool, sizeof...(extraFds)> extraResult{};
      for (std::size_t i = 0; i < sizeof...(extraFds); i++) {
        extraResult[i] = unistdpp::canRead(fds[i]); // NOLINT
      }

      return std::pair{ res, extraResult };
    }
  }

  BaseDevices getBaseDevices() const { return baseDevices; }

  size_t numDevices() const { return devices.size(); }
  void removeDevice(std::string_view path) {
    auto it = devices.find(path);
    if (it == devices.end()) {
      return;
    }

    if (it->second.get() == baseDevices.pogoKeyboard) {
      baseDevices.pogoKeyboard = nullptr;
    }

    devices.erase(it);
  }

private:
  /// members
  std::unordered_map<std::string_view, std::unique_ptr<InputDeviceBase>>
    devices;

  BaseDevices baseDevices;
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
