#pragma once

#include "Message.h"

#include <memory>

struct libevdev_uinput;

struct UinputDeleter {
  void operator()(libevdev_uinput*);
};
using UinputPtr = std::unique_ptr<libevdev_uinput, UinputDeleter>;

UinputPtr
makeWacomDevice();

UinputPtr
makeTouchDevice();

UinputPtr
makeButtonDevice();

struct AllUinputDevices {
  UinputPtr wacom;
  UinputPtr touch;
  UinputPtr button;
};

AllUinputDevices
makeAllDevices();

void
sendInput(const Input& input, libevdev_uinput& wacomDevice);
