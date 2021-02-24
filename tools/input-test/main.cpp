#include <Device.h>
#include <Input.h>

#include <iostream>

#include <libudev.h>

using namespace rmlib;
using namespace rmlib::input;

constexpr auto subsystem = "input";

static void
print_device(struct udev_device* dev) {
  const char* action = udev_device_get_action(dev);
  if (!action)
    action = "exists";

  const char* vendor = udev_device_get_sysattr_value(dev, "idVendor");
  if (!vendor)
    vendor = "0000";

  const char* product = udev_device_get_sysattr_value(dev, "idProduct");
  if (!product)
    product = "0000";

  printf("%s %s %6s %s:%s %s\n",
         udev_device_get_subsystem(dev),
         udev_device_get_devtype(dev),
         action,
         vendor,
         product,
         udev_device_get_devnode(dev));
}

static void
process_device(struct udev_device* dev) {
  if (dev) {
    if (udev_device_get_devnode(dev)) {
      print_device(dev);
    }

    udev_device_unref(dev);
  }
}

static void
enumerate_devices(struct udev* udev) {
  struct udev_enumerate* enumerate = udev_enumerate_new(udev);

  udev_enumerate_add_match_subsystem(enumerate, subsystem);
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, devices) {
    const char* path = udev_list_entry_get_name(entry);
    struct udev_device* dev = udev_device_new_from_syspath(udev, path);
    process_device(dev);
  }

  udev_enumerate_unref(enumerate);
}

static void
monitor_devices(struct udev* udev) {
  struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");

  udev_monitor_filter_add_match_subsystem_devtype(mon, subsystem, NULL);
  udev_monitor_enable_receiving(mon);

  int fd = udev_monitor_get_fd(mon);

  while (1) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int ret = select(fd + 1, &fds, NULL, NULL, NULL);
    if (ret <= 0)
      break;

    if (FD_ISSET(fd, &fds)) {
      struct udev_device* dev = udev_monitor_receive_device(mon);
      process_device(dev);
    }
  }
}

void
printEvent(const TouchEvent& ev) {
  std::cout << "Touch ";
  switch (ev.type) {
    case TouchEvent::Down:
      std::cout << "Down";
      break;
    case TouchEvent::Up:
      std::cout << "Up";
      break;
    case TouchEvent::Move:
      std::cout << "Move";
      break;
  }
  std::cout << " at " << ev.location.x << "x" << ev.location.y;
  std::cout << " id " << ev.id << " slot " << ev.slot << std::endl;
}

void
printEvent(const PenEvent& ev) {
  std::cout << "Pen ";
  switch (ev.type) {
    case PenEvent::ToolClose:
      std::cout << "ToolClose";
      break;
    case PenEvent::ToolLeave:
      std::cout << "ToolLeave";
      break;
    case PenEvent::TouchDown:
      std::cout << "TouchDown";
      break;
    case PenEvent::TouchUp:
      std::cout << "TouchUp";
      break;
    case PenEvent::Move:
      std::cout << "Move";
      break;
  }
  std::cout << " at " << ev.location.x << "x" << ev.location.y;
  std::cout << " dist " << ev.distance << " pres " << ev.pressure << std::endl;
}

void
printEvent(const KeyEvent& ev) {
  std::cout << "Key ";
  switch (ev.type) {
    case KeyEvent::Press:
      std::cout << "Press ";
      break;
    case KeyEvent::Release:
      std::cout << "Release ";
      break;
    case KeyEvent::Repeat:
      std::cout << "Repeat ";
      break;
  }
  std::cout << ev.keyCode << std::endl;
}

int
main() {
  // struct udev* udev = udev_new();
  // if (!udev) {
  //   fprintf(stderr, "udev_new() failed\n");
  //   return 1;
  // }

  // enumerate_devices(udev);
  // monitor_devices(udev);

  // udev_unref(udev);

  // auto deviceType = device::getDeviceType();
  // if (deviceType.isError()) {
  //   std::cerr << "Unknown device\n";
  //   return -1;
  // }

  InputManager input;
  auto err = input.openAll();
  if (err.isError()) {
    std::cerr << err.getError().msg << std::endl;
  }

  while (true) {
    auto events = input.waitForInput(std::nullopt);
    if (events.isError()) {
      std::cerr << "Reading input error: " << events.getError().msg << "\n";
      continue;
    }

    for (auto& event : *events) {
      std::visit([](auto& e) { printEvent(e); }, event);
    }
  }

  return 0;
}
