#include "InputDevice.h"

#include <libevdev/libevdev-uinput.h>

#include <iostream>

void
UinputDeleter::operator()(libevdev_uinput* device) {
  libevdev_uinput_destroy(device);
}

UinputPtr
makeWacomDevice() {
  auto* dev = libevdev_new();

  libevdev_set_name(dev, "Wacom I2C Digitizer");
  libevdev_enable_event_type(dev, EV_SYN);

  libevdev_enable_event_type(dev, EV_KEY);
  libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_PEN, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_RUBBER, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_TOUCH, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_STYLUS, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, BTN_STYLUS2, nullptr);

  libevdev_enable_event_type(dev, EV_ABS);
  input_absinfo info = {};
  info.minimum = 0;
  info.maximum = 20966;
  info.resolution = 100;
  libevdev_enable_event_code(dev, EV_ABS, ABS_X, &info);
  info.minimum = 0;
  info.maximum = 15725;
  libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &info);
  info.resolution = 0;

  info.minimum = 0;
  info.maximum = 4095;
  libevdev_enable_event_code(dev, EV_ABS, ABS_PRESSURE, &info);

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_DISTANCE, &info);

  info.minimum = -9000;
  info.maximum = 9000;
  libevdev_enable_event_code(dev, EV_ABS, ABS_TILT_X, &info);
  libevdev_enable_event_code(dev, EV_ABS, ABS_TILT_Y, &info);

  libevdev_uinput* uidev = nullptr;
  auto err = libevdev_uinput_create_from_device(
    dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
  if (err != 0) {
    perror("uintput");
    std::cerr << "Error making uinput device\n";
    return nullptr;
  }

  std::cout << "Added uintput device\n";

  return UinputPtr(uidev);
}

UinputPtr
makeTouchDevice() {
  auto* dev = libevdev_new();

  libevdev_set_name(dev, "pt_mt");
  libevdev_enable_event_type(dev, EV_SYN);

  libevdev_enable_event_type(dev, EV_KEY);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F1, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F2, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F3, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F4, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F5, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F6, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F7, nullptr);
  libevdev_enable_event_code(dev, EV_KEY, KEY_F8, nullptr);

  libevdev_enable_event_type(dev, EV_REL);
  libevdev_enable_event_type(dev, EV_ABS);
  input_absinfo info = {};
  info.resolution = 0;

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_DISTANCE, &info);

  info.minimum = 0;
  info.maximum = 31;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_SLOT, &info);

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, &info);
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TOUCH_MINOR, &info);

  info.minimum = -127;
  info.maximum = 127;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_ORIENTATION, &info);

  info.minimum = 0;
  info.maximum = 1403;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_X, &info);
  info.minimum = 0;
  info.maximum = 1871;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_Y, &info);

  info.minimum = 0;
  info.maximum = 1;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TOOL_TYPE, &info);

  info.minimum = 0;
  info.maximum = 65535;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID, &info);

  info.minimum = 0;
  info.maximum = 255;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_PRESSURE, &info);

  libevdev_uinput* uidev = nullptr;
  auto err = libevdev_uinput_create_from_device(
    dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
  if (err != 0) {
    perror("uintput");
    std::cerr << "Error making uinput device\n";
    return nullptr;
  }

  std::cout << "Added uintput device\n";

  return UinputPtr(uidev);
}

UinputPtr
makeButtonDevice() {
  auto* dev = libevdev_new();

  libevdev_set_name(dev, "30371337.snvs:snvs-powerkey");
  libevdev_enable_event_type(dev, EV_SYN);

  libevdev_enable_event_type(dev, EV_KEY);
  libevdev_enable_event_code(dev, EV_KEY, KEY_POWER, nullptr);

  libevdev_uinput* uidev = nullptr;
  auto err = libevdev_uinput_create_from_device(
    dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
  if (err != 0) {
    perror("uintput");
    std::cerr << "Error making uinput device\n";
    return nullptr;
  }

  std::cout << "Added uintput device\n";

  return UinputPtr(uidev);
}

AllUinputDevices
makeAllDevices() {
  AllUinputDevices result;

  result.button = makeButtonDevice();
  result.wacom = makeWacomDevice();
  result.touch = makeTouchDevice();

  return result;
}

void
sendInput(const Input& input, libevdev_uinput& wacomDevice) {
  constexpr auto wacom_width = 15725;
  constexpr auto wacom_height = 20967;

  constexpr auto screen_width = 1404;
  constexpr auto screen_height = 1872;

  auto x = int(float(input.x) * wacom_width / screen_width);
  auto y = int(wacom_height - float(input.y) * wacom_height / screen_height);

  libevdev_uinput_write_event(&wacomDevice, EV_ABS, ABS_X, y);
  libevdev_uinput_write_event(&wacomDevice, EV_ABS, ABS_Y, x);

  if (input.type != 0) {
    const auto value = input.type == 1 ? 1 : 0;
    libevdev_uinput_write_event(&wacomDevice, EV_KEY, BTN_TOOL_PEN, value);
    libevdev_uinput_write_event(&wacomDevice, EV_KEY, BTN_TOUCH, value);
  }

  libevdev_uinput_write_event(&wacomDevice, EV_SYN, SYN_REPORT, 0);
}
