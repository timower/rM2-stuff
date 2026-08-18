#include "Power.h"

#include <unistdpp/file.h>

#include <iostream>

namespace {

bool
wokenByUser() {
  // No wakeup-irq file means the power button (not some other hardware
  // source) is what woke the device.
  return !unistdpp::readFile("/sys/power/pm_wakeup_irq").has_value();
}

} // namespace

unistdpp::Result<SystemPowerInterface>
SystemPowerInterface::open() {
  auto bus = TRY(systemdpp::Bus::openSystem());
  TRY(bus.matchSignal(
    login1::destination, login1::path, login1::interface, "PrepareForSleep"));
  return SystemPowerInterface(std::move(bus));
}

std::optional<rmlib::device::BatteryInfo>
SystemPowerInterface::getBattery() {
  return rmlib::device::getBatteryInfo();
}

void
SystemPowerInterface::powerOff() {
#ifdef EMULATE
  return;
#endif
  bus
    .callMethod(
      login1::destination, login1::path, login1::interface, "PowerOff", "b", 0)
    .or_else([](auto errc) {
      std::cerr << "Error powering off: " << unistdpp::to_string(errc) << "\n";
    });
}

void
SystemPowerInterface::reboot() {
#ifdef EMULATE
  return;
#endif
  bus
    .callMethod(
      login1::destination, login1::path, login1::interface, "Reboot", "b", 0)
    .or_else([](auto errc) {
      std::cerr << "Error rebooting: " << unistdpp::to_string(errc) << "\n";
    });
}

void
SystemPowerInterface::softReboot() {
#ifdef EMULATE
  return;
#endif
  // SD_LOGIND_SOFT_REBOOT, see logind's org.freedesktop.login1.Manager
  // RebootWithFlags() docs - goes through the same polkit-gated path as a
  // regular Reboot(), unlike calling systemd1.Manager.SoftReboot directly.
  constexpr uint64_t sdLogindSoftReboot = UINT64_C(1) << 2;
  bus
    .callMethod(login1::destination,
                login1::path,
                login1::interface,
                "RebootWithFlags",
                "t",
                sdLogindSoftReboot)
    .or_else([](auto errc) {
      std::cerr << "Error soft-rebooting: " << unistdpp::to_string(errc)
                << "\n";
    });
}

int
SystemPowerInterface::sleepFd() {
  return bus.fd();
}

std::optional<PowerInterface::SleepUpdate>
SystemPowerInterface::pollSleep() {
  // Returns after the first PrepareForSleep match rather than draining the
  // whole batch, so a queued true+false pair (e.g. woken up after the
  // process itself was frozen for the actual suspend) isn't collapsed into
  // just the last one - the caller is expected to call this again to pick
  // up anything left over.
  for (;;) {
    auto processed = bus.process();
    if (!processed.has_value()) {
      std::cerr << "Error processing sleep bus: "
                << unistdpp::to_string(processed.error()) << "\n";
      return std::nullopt;
    }

    if (processed->message.has_value() &&
        processed->message->isSignal(login1::interface, "PrepareForSleep")) {
      auto sleeping = processed->message->readBool();
      if (sleeping.has_value()) {
        return SleepUpdate{ .sleeping = *sleeping,
                            .wokenByUser = *sleeping ? false : wokenByUser() };
      }
    }

    if (!processed->hasMore) {
      return std::nullopt;
    }
  }
}

bool
SystemPowerInterface::requestSuspend() {
#ifdef EMULATE
  return false;
#endif
  // Async: a blocking call here would deadlock us against ourselves - its
  // reply only arrives after we've released our own sleep delay lock, which
  // requires processing the PrepareForSleep signal this same call triggers.
  auto result = bus.callMethodAsync(
    login1::destination, login1::path, login1::interface, "Suspend", "b", 1);
  if (!result.has_value()) {
    std::cerr << "Error requesting suspend: "
              << unistdpp::to_string(result.error()) << "\n";
    return false;
  }
  return true;
}

unistdpp::Result<unistdpp::FD>
SystemPowerInterface::acquireSleepLock() {
  return bus
    .callMethod(login1::destination,
                login1::path,
                login1::interface,
                "Inhibit",
                "ssss",
                "sleep",
                "rocket",
                "Inhibit sleep from rocket",
                "delay")
    .and_then([](auto msg) { return msg.readFD(); });
}

unistdpp::Result<unistdpp::FD>
SystemPowerInterface::acquireIdleLock() {
  return bus
    .callMethod(login1::destination,
                login1::path,
                login1::interface,
                "Inhibit",
                "ssss",
                "idle",
                "rocket",
                "Inhibit idle from rocket",
                "block")
    .and_then([](auto msg) { return msg.readFD(); });
}
