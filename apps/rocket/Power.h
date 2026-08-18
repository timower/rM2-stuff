#pragma once

#include <Device.h>

#include <systemdpp/sdbus.h>

#include <optional>

namespace login1 {
constexpr auto destination = "org.freedesktop.login1";
constexpr auto path = "/org/freedesktop/login1";
constexpr auto interface = "org.freedesktop.login1.Manager";
} // namespace login1

struct PowerInterface {
  virtual ~PowerInterface() = default;

  virtual std::optional<rmlib::device::BatteryInfo> getBattery() = 0;
  virtual void powerOff() = 0;

  // Hardware reboot - the device boots back into xochitl, nixos has no
  // bootloader entry of its own.
  virtual void reboot() = 0;

  // systemd soft-reboot - re-execs into nixos' own root again without a
  // real hardware reboot, staying in nixos.
  virtual void softReboot() = 0;

  struct SleepUpdate {
    bool sleeping;    // true right before sleep, false right after resume.
    bool wokenByUser; // only meaningful when !sleeping.
  };

  // Pollable fd for suspend/resume bus traffic (-1 if unavailable). Pass to
  // the caller's own event loop; call pollSleep() once it's readable.
  virtual int sleepFd() = 0;

  // Drains pending suspend/resume bus traffic, returning the transition
  // observed, if any.
  virtual std::optional<SleepUpdate> pollSleep() = 0;

  // Requests that the system suspend now. Returns false if the request
  // itself couldn't be made - callers can't wait for a PrepareForSleep
  // signal in that case, since none will ever arrive.
  virtual bool requestSuspend() = 0;

  // Acquires the "sleep" delay lock and hands the caller the fd - holding
  // it off delays the actual suspend, closing it releases the hold. Must
  // be re-acquired after every resume.
  virtual unistdpp::Result<unistdpp::FD> acquireSleepLock() = 0;

  // Acquires the "idle" block lock, held while the launcher wants to
  // prevent logind's own idle-based suspend. Caller owns the fd.
  virtual unistdpp::Result<unistdpp::FD> acquireIdleLock() = 0;
};

struct SystemPowerInterface : PowerInterface {
  static unistdpp::Result<SystemPowerInterface> open();

  std::optional<rmlib::device::BatteryInfo> getBattery() override;
  void powerOff() override;
  void reboot() override;
  void softReboot() override;

  int sleepFd() override;
  std::optional<SleepUpdate> pollSleep() override;
  bool requestSuspend() override;
  unistdpp::Result<unistdpp::FD> acquireSleepLock() override;
  unistdpp::Result<unistdpp::FD> acquireIdleLock() override;

private:
  explicit SystemPowerInterface(systemdpp::Bus bus) : bus(std::move(bus)) {}

  systemdpp::Bus bus;
};
