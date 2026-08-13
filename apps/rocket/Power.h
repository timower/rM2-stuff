#pragma once

#include <Device.h>

#include <optional>

struct PowerInterface {
  virtual ~PowerInterface() = default;

  virtual std::optional<rmlib::device::BatteryInfo> getBattery() = 0;

  // Suspends the device, returns true if woken by the user (power button).
  virtual bool suspend() = 0;

  virtual void powerOff() = 0;
};

struct SystemPowerInterface : PowerInterface {
  std::optional<rmlib::device::BatteryInfo> getBattery() override;
  bool suspend() override;
  void powerOff() override;
};
