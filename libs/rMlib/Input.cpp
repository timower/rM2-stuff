#include "Input.h"

#include "Device.h"

#include <unistdpp/file.h>
#include <unistdpp/poll.h>

#include <libevdev/libevdev.h>
#include <libudev.h>

#include <linux/input.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>

namespace rmlib::input {

namespace {

constexpr auto touch_flood_size = 8 * 512 * 4;

auto*
getTouchFlood() {
  static const auto* floodBuffer = [] {
    // NOLINTNEXTLINE
    static const auto ret = std::make_unique<input_event[]>(touch_flood_size);

    constexpr auto mk_input_ev = [](int a, int b, int v) {
      input_event r{};
      r.type = a;
      r.code = b;
      r.value = v;
      r.time = { 0, 0 };
      return r;
    };

    for (int i = 0; i < touch_flood_size;) {
      ret[i++] = mk_input_ev(EV_ABS, ABS_DISTANCE, 1);
      ret[i++] = mk_input_ev(EV_SYN, 0, 0);
      ret[i++] = mk_input_ev(EV_ABS, ABS_DISTANCE, 2);
      ret[i++] = mk_input_ev(EV_SYN, 0, 0);
    }

    return ret.get();
  }();
  return floodBuffer;
}

template<typename Device>
struct InputDevice : public InputDeviceBase {
  using InputDeviceBase::InputDeviceBase;

  virtual OptError<> readEvents(std::vector<Event>& out) final {
    Device* devThis = static_cast<Device*>(this);

    int rc = 0;
    do {

      auto event = input_event{};
      rc = libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_NORMAL, &event);

      if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
        auto evs = TRY(devThis->handleEvent(event));
        out.insert(out.end(), evs.begin(), evs.end());
      } else if (rc == LIBEVDEV_READ_STATUS_SYNC) {

        while (rc == LIBEVDEV_READ_STATUS_SYNC) {
          auto evs = TRY(devThis->handleEvent(event));
          out.insert(out.end(), evs.begin(), evs.end());

          rc = libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_SYNC, &event);
        }
      }
    } while (rc == LIBEVDEV_READ_STATUS_SYNC ||
             rc == LIBEVDEV_READ_STATUS_SUCCESS);

    return {};
  }
};

struct TouchDevice : public InputDevice<TouchDevice> {
  TouchDevice(unistdpp::FD fd,
              libevdev* evdev,
              std::string path,
              Transform transform)
    : InputDevice(std::move(fd), evdev, std::move(path))
    , transform(transform) {}
  ErrorOr<std::vector<TouchEvent>> handleEvent(input_event);
  TouchEvent& getSlot() { return slots.at(slot); }

  void flood() final {
    auto* buf = getTouchFlood();
    (void)fd.writeAll(buf, touch_flood_size * sizeof(input_event));
  }

  Transform transform;
  int slot = 0;
  std::array<TouchEvent, max_num_slots> slots;
  std::unordered_set<int> changedSlots;
};

struct PenDevice : public InputDevice<PenDevice> {
  PenDevice(unistdpp::FD fd,
            libevdev* evdev,
            std::string path,
            Transform transform)
    : InputDevice(std::move(fd), evdev, std::move(path))
    , transform(transform) {}
  ErrorOr<std::vector<PenEvent>> handleEvent(input_event);

  void flood() final {
    auto* buf = getTouchFlood();
    (void)fd.writeAll(buf, touch_flood_size * sizeof(input_event));
  }

  Transform transform;
  PenEvent penEvent;
};

struct KeyDevice : public InputDevice<KeyDevice> {
  KeyDevice(unistdpp::FD fd, libevdev* evdev, std::string path)
    : InputDevice(std::move(fd), evdev, std::move(path)) {}
  ErrorOr<std::vector<KeyEvent>> handleEvent(input_event);

  void flood() final {
    // TODO: this probably doesn't work
    auto* buf = getTouchFlood();
    (void)fd.writeAll(buf, touch_flood_size * sizeof(input_event));
  }

  std::vector<KeyEvent> keyEvents;
};

ErrorOr<std::vector<PenEvent>>
PenDevice::handleEvent(input_event event) {
  if (event.type == EV_SYN && event.code == SYN_REPORT) {
    auto ev = penEvent;
    ev.location = transform * penEvent.location;
    std::vector<PenEvent> events{ ev };

    penEvent.type = PenEvent::Move;
    return events;
  }

  if (event.type == EV_ABS) {
    if (event.code == ABS_X) {
      penEvent.location.x = event.value;
    } else if (event.code == ABS_Y) {
      penEvent.location.y = event.value;
    } else if (event.code == ABS_DISTANCE) {
      penEvent.distance = event.value;
    } else if (event.code == ABS_PRESSURE) {
      penEvent.pressure = event.value;
    }
  } else if (event.type == EV_KEY) {
    if (event.code == BTN_TOOL_PEN) {
      if (event.value == KeyEvent::Press) {
        penEvent.type = PenEvent::ToolClose;
      } else {
        penEvent.type = PenEvent::ToolLeave;
      }
    } else if (event.code == BTN_TOUCH) {
      if (event.value == KeyEvent::Press) {
        penEvent.type = PenEvent::TouchDown;
      } else {
        penEvent.type = PenEvent::TouchUp;
      }
    }
  }

  return std::vector<PenEvent>{};
}

ErrorOr<std::vector<TouchEvent>>
TouchDevice::handleEvent(input_event event) {
  if (event.type == EV_SYN && event.code == SYN_REPORT) {
    std::vector<TouchEvent> events;
    for (auto slotIdx : changedSlots) {
      auto slot = slots.at(slotIdx);
      slot.location = transform * slot.location;
      events.push_back(slot);

      slots.at(slotIdx).type = TouchEvent::Move;
    }
    changedSlots.clear();
    return events;
  }

  if (event.type == EV_ABS && event.code == ABS_MT_SLOT) {
    slot = event.value;
    getSlot().slot = event.value;
  }
  changedSlots.insert(slot);

  if (event.type == EV_ABS) {
    if (event.code == ABS_MT_TRACKING_ID) {
      auto& slot = getSlot();
      if (event.value == -1) {
        slot.type = TouchEvent::Up;
        // setType = true;
      } else {
        slot.type = TouchEvent::Down;
        slot.id = event.value;
        // setType = true;
      }
    } else if (event.code == ABS_MT_POSITION_X) {
      auto& slot = getSlot();
      slot.location.x = event.value;
    } else if (event.code == ABS_MT_POSITION_Y) {
      auto& slot = getSlot();
      slot.location.y = event.value;
    } else if (event.code == ABS_MT_PRESSURE) {
      auto& slot = getSlot();
      slot.pressure = event.value;
    }
  }

  return std::vector<TouchEvent>{};
}

ErrorOr<std::vector<KeyEvent>>
KeyDevice::handleEvent(input_event event) {
  if (event.type == EV_KEY) {

    KeyEvent keyEvent;
    keyEvent.type = static_cast<decltype(keyEvent.type)>(event.value);
    keyEvent.keyCode = event.code;
    keyEvents.push_back(keyEvent);

  } else if (event.type == EV_SYN && event.code == SYN_REPORT) {

    auto ret = keyEvents;
    keyEvents.clear();
    return ret;
  }

  return std::vector<KeyEvent>{};
}

std::unique_ptr<InputDeviceBase>
makeDevice(unistdpp::FD fd,
           libevdev* evdev,
           std::string path,
           Transform transform = {}) {
  if (libevdev_has_event_type(evdev, EV_ABS)) {
    // multi-touch screen -> ev_abs & abs_mt_slot
    if (libevdev_has_event_code(evdev, EV_ABS, ABS_MT_SLOT)) {
      std::cout << "Got touch\n";
      return std::make_unique<TouchDevice>(
        std::move(fd), evdev, std::move(path), transform);
    }

    // pen/single touch screen -> ev_abs
    if (libevdev_has_event_code(evdev, EV_ABS, ABS_X)) {
      std::cout << "Got pen\n";
      return std::make_unique<PenDevice>(
        std::move(fd), evdev, std::move(path), transform);
    }
  }

  std::cout << "Got key\n";
  // finally keyboard -> ev_key
  // TODO: support mouse?
  return std::make_unique<KeyDevice>(std::move(fd), evdev, std::move(path));
}

void
handeDevice(InputManager& mgr, udev_device& dev) {
  auto* devnode = udev_device_get_devnode(&dev);
  if (devnode == nullptr) {
    return;
  }

  auto* action = udev_device_get_action(&dev);
  std::cout << "action: " << (action == nullptr ? "null" : action) << "\n";

  if (action == nullptr || action == std::string_view("add")) {
    mgr.open(devnode);
    return;
  }
  mgr.devices.erase(devnode);
}

} // namespace

template<>
void
InputManager::UdevDeleter<udev>::operator()(udev* ptr) {
  udev_unref(ptr);
}

template<>
void
InputManager::UdevDeleter<udev_monitor>::operator()(udev_monitor* ptr) {
  udev_monitor_unref(ptr);
}

const char*
InputDeviceBase::getName() {
  return libevdev_get_name(evdev);
}

void
InputDeviceBase::grab() {
  libevdev_grab(evdev, LIBEVDEV_GRAB);
}

void
InputDeviceBase::ungrab() {
  libevdev_grab(evdev, LIBEVDEV_UNGRAB);
}

InputDeviceBase::~InputDeviceBase() {
  if (evdev != nullptr) {
    libevdev_free(evdev);
  }
}

ErrorOr<InputDeviceBase*>
InputManager::open(std::string_view input) {
  if (auto it = devices.find(input); it != devices.end()) {
    return &*it->second;
  }

  auto fd = TRY(unistdpp::open(input.data(), O_RDWR | O_NONBLOCK));

  libevdev* dev = nullptr;
  if (libevdev_new_from_fd(fd.fd, &dev) < 0) {
    return Error::make("Error initializing evdev for '" + std::string(input) +
                       "'");
  }

  auto baseTransform = [&]() -> Transform {
    auto base = device::getBaseDevice(libevdev_get_name(dev));
    if (!base) {
      return {};
    }
    return base->transform;
  }();

  auto device =
    makeDevice(std::move(fd), dev, std::string(input), baseTransform);
  auto* devicePtr = device.get();
  devices.emplace(devicePtr->path, std::move(device));
  return devicePtr;
}

ErrorOr<BaseDevices>
InputManager::openAll(bool monitor) {
  auto udevHandle = this->udevHandle == nullptr ? UniquePtr<udev>(udev_new())
                                                : std::move(this->udevHandle);

  udev_enumerate* enumerate = udev_enumerate_new(udevHandle.get());
  udev_enumerate_add_match_subsystem(enumerate, "input");
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry* devList = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, devList) {
    const char* path = udev_list_entry_get_name(entry);
    struct udev_device* dev =
      udev_device_new_from_syspath(udevHandle.get(), path);
    if (dev != nullptr) {
      handeDevice(*this, *dev);
      udev_device_unref(dev);
    }
  }

  udev_enumerate_unref(enumerate);

  if (monitor) {
    this->udevHandle = std::move(udevHandle);

    udevMonitor = UniquePtr<udev_monitor>(
      udev_monitor_new_from_netlink(udevHandle.get(), "udev"));

    udev_monitor_filter_add_match_subsystem_devtype(
      udevMonitor.get(), "input", NULL);
    udev_monitor_enable_receiving(udevMonitor.get());

    udevMonitorFd = unistdpp::FD(udev_monitor_get_fd(udevMonitor.get()));

  } else {
    this->udevMonitorFd = unistdpp::FD{};
  }

  for (const auto& [_, dev] : devices) {
    auto baseDev = device::getBaseDevice(dev->getName());
    if (!baseDev) {
      continue;
    }
    switch (baseDev->type) {
      case device::InputType::Key:
        baseDevices.key = dev.get();
        break;
      case device::InputType::Pen:
        baseDevices.pen = dev.get();
        break;
      case device::InputType::MultiTouch:
        baseDevices.touch = dev.get();
        break;
    }
  }

  return baseDevices;
}

ErrorOr<std::vector<Event>>
InputManager::waitForInput(std::vector<pollfd>& extraFds,
                           std::optional<std::chrono::milliseconds> timeout) {
  const auto inputFdsSize = extraFds.size();

  std::vector<InputDeviceBase*> deviceOrder;
  for (auto& [_, device] : devices) {
    (void)_;
    deviceOrder.emplace_back(device.get());
    extraFds.emplace_back(unistdpp::waitFor(device->fd, unistdpp::Wait::READ));
  }

  if (udevMonitorFd.isValid()) {
    extraFds.emplace_back(
      unistdpp::waitFor(udevMonitorFd, unistdpp::Wait::READ));
  }

  auto ret = TRY(unistdpp::poll(extraFds, timeout));
  if (ret == 0) {
    // timeout
    return std::vector<Event>{};
  }

  if (udevMonitorFd.isValid() && unistdpp::canRead(extraFds.back())) {
    udev_device* dev = udev_monitor_receive_device(udevMonitor.get());
    if (dev) {
      handeDevice(*this, *dev);
      udev_device_unref(dev);
    }
  }

  // Return the first device we see.
  std::vector<Event> result;
  for (std::size_t i = 0; i < deviceOrder.size(); i++) {
    auto& device = *deviceOrder[i];
    if (unistdpp::canRead(extraFds[i + inputFdsSize])) {
      TRY(device.readEvents(result));
    }
  }

  extraFds.resize(inputFdsSize);

  return result;
}
} // namespace rmlib::input
