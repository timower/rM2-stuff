#pragma once

#include "rm2fb/Message.h"

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
sendPen(const Input& input, libevdev_uinput& wacomDevice);

void
sendTouch(const Input& input, libevdev_uinput& touchDevice);

void
sendButton(bool down, libevdev_uinput& buttonDevice);
