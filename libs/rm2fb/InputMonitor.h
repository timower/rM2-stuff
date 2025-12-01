#pragma once

#include "unistdpp/file.h"
#include "unistdpp/unistdpp.h"
#include <libevdev/libevdev.h>
#include <libudev.h>
#include <memory>

constexpr auto touch_flood_size = 8 * 512 * 4;
constexpr auto key_flood_size = 8 * 64;

auto
mkEvent(int a, int b, int v) {
  input_event r{};
  r.type = a;
  r.code = b;
  r.value = v;
  r.input_event_sec = 0;
  r.input_event_usec = 0;
  return r;
}

auto*
getTouchFlood() {
  static const auto* floodBuffer = [] {
    // NOLINTNEXTLINE
    static const auto ret = std::make_unique<input_event[]>(touch_flood_size);
    for (int i = 0; i < touch_flood_size;) {
      ret[i++] = mkEvent(EV_ABS, ABS_DISTANCE, 1);
      ret[i++] = mkEvent(EV_SYN, 0, 0);
      ret[i++] = mkEvent(EV_ABS, ABS_DISTANCE, 2);
      ret[i++] = mkEvent(EV_SYN, 0, 0);
    }
    return ret.get();
  }();
  return floodBuffer;
}

auto*
getKeyFlood() {
  static const auto* floodBuffer = [] {
    // NOLINTNEXTLINE
    static const auto ret = std::make_unique<input_event[]>(key_flood_size);
    for (int i = 0; i < key_flood_size;) {
      ret[i++] = mkEvent(EV_KEY, KEY_LEFTALT, 1);
      ret[i++] = mkEvent(EV_SYN, SYN_REPORT, 0);
      ret[i++] = mkEvent(EV_KEY, KEY_LEFTALT, 0);
      ret[i++] = mkEvent(EV_SYN, SYN_REPORT, 0);
    }
    return ret.get();
  }();
  return floodBuffer;
}
struct InputMonitor {
  ~InputMonitor() {
    if (udevMonitor != nullptr) {
      udev_monitor_unref(udevMonitor);
    }
    if (udevHandle != nullptr) {
      udev_unref(udevHandle);
    }
  }

  void openDevices() {
    udevHandle = udev_new();

    udev_enumerate* enumerate = udev_enumerate_new(udevHandle);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry* devList = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry = nullptr;

    udev_list_entry_foreach(entry, devList) {
      const char* path = udev_list_entry_get_name(entry);
      struct udev_device* dev = udev_device_new_from_syspath(udevHandle, path);

      if (dev != nullptr) {
        handleUdev(*dev);
        udev_device_unref(dev);
      }
    }

    udev_enumerate_unref(enumerate);
  }

  void startMonitor() {
    udevMonitor = udev_monitor_new_from_netlink(udevHandle, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(
      udevMonitor, "input", nullptr);
    udev_monitor_enable_receiving(udevMonitor);
    udevMonitorFd = unistdpp::FD(udev_monitor_get_fd(udevMonitor));
  }

  void handleNewDevices() {
    auto* dev = udev_monitor_receive_device(udevMonitor);
    if (dev != nullptr) {
      handleUdev(*dev);
      udev_device_unref(dev);
    }
  }

  void flood() {
    for (const auto& [_, dev] : devices) {
      if (dev.isTouch) {
        (void)dev.fd.writeAll(getTouchFlood(),
                              touch_flood_size * sizeof(input_event));
      } else {
        (void)dev.fd.writeAll(getKeyFlood(),
                              key_flood_size * sizeof(input_event));
      }
    }
  }

  void openDevice(std::string input) {
    if (devices.count(input) != 0) {
      return;
    }

    auto fd = unistdpp::open(input.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (!fd) {
      return;
    }

    libevdev* dev;
    if (libevdev_new_from_fd(fd->fd, &dev) < 0) {
      return;
    }

    if (libevdev_has_event_type(dev, EV_ABS) != 0) {
      // multi-touch screen -> ev_abs & abs_mt_slot
      if (libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT) != 0) {
        std::cerr << "Tracking MT dev: " << input << "\n";
        devices[input] = Device{ std::move(*fd), true };
      }
      // Ignore pen input
    } else if (libevdev_has_event_type(dev, EV_KEY) != 0) {
      std::cerr << "Tracking key dev: " << input << "\n";
      devices[input] = Device{ std::move(*fd), false };
    }

    libevdev_free(dev);
  }

  void handleUdev(udev_device& dev) {
    const auto* devnode = udev_device_get_devnode(&dev);
    if (devnode == nullptr) {
      return;
    }

    const auto* action = udev_device_get_action(&dev);
    if (action == nullptr || action == std::string_view("add")) {
      openDevice(devnode);
      return;
    }

    devices.erase(devnode);
  }

  udev* udevHandle = nullptr;
  udev_monitor* udevMonitor = nullptr;

  unistdpp::FD udevMonitorFd;

  struct Device {
    unistdpp::FD fd;
    bool isTouch;
  };
  std::unordered_map<std::string, Device> devices;
};
