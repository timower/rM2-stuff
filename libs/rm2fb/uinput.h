#pragma once

#include <cstdint>
#include <memory>

struct libevdev_uinput;

struct Input {
  int32_t x;
  int32_t y;
  int32_t type; // 1 = down, 2 = up
};

static_assert(sizeof(Input) == 3 * 4, "Input message has unexpected size");

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
