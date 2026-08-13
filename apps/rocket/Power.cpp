#include "Power.h"

#include <systemdpp/sdbus.h>
#include <unistdpp/file.h>

#include <iostream>

std::optional<rmlib::device::BatteryInfo>
SystemPowerInterface::getBattery() {
  return rmlib::device::getBatteryInfo();
}

bool
SystemPowerInterface::suspend() {
  if (!systemdpp::waitForSleep()) {
    return false;
  }

  // Get the reason
  auto irq = unistdpp::readFile("/sys/power/pm_wakeup_irq");
  if (!irq.has_value()) {
    std::cout << "Error getting reason: " << unistdpp::to_string(irq.error())
              << std::endl;

    // If there is no irq it must be the user which pressed the button:
    return true;
  }
  std::cout << "Reason for wake irq: " << *irq << std::endl;
  return false;
}

void
SystemPowerInterface::powerOff() {
  systemdpp::powerOff();
}
