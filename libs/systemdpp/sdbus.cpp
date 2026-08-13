#include <systemdpp/sdbus.h>

#include <unistdpp/file.h>

#ifdef __linux__
#include <systemd/sd-bus.h>
#endif

namespace systemdpp {

bool
waitForSleep() {
#ifdef __linux__
  bool inSleep = false;
  int res = 0;
  sd_bus* bus = nullptr;
  res = sd_bus_open_system(&bus);
  if (res < 0) {
    std::cerr << "Error opening system bus: " << strerror(-res) << "\n";
    return false;
  }

  res = sd_bus_match_signal(
    bus,
    nullptr,
    "org.freedesktop.login1",
    "/org/freedesktop/login1",
    "org.freedesktop.login1.Manager",
    "PrepareForSleep",
    [](sd_bus_message* m, void* inSleep, sd_bus_error*) -> int {
      int sleeping;
      int res = sd_bus_message_read(m, "b", &sleeping);
      if (res < 0) {
        std::cerr << "Error reading message: " << strerror(-res) << "\n";
        return res;
      }

      std::cout << "Sleep got: " << sleeping << "\n";

      if (!sleeping) {
        // Waking up
        *(bool*)inSleep = true;
      }
      return 0;
    },
    &inSleep);
  if (res < 0) {
    std::cerr << "Error subscribing to signal: " << strerror(-res) << "\n";
    sd_bus_unref(bus);
    return false;
  }

  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message* reply = nullptr;
  res = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           "Suspend",
                           &error,
                           &reply,
                           "b",
                           /* interactive */ 1);
  if (res < 0) {
    std::cerr << "Error suspending: " << strerror(-res) << "\n";
    sd_bus_unref(bus);
    return false;
  }
  sd_bus_message_unref(reply);

  // Wait for suspend signal
  while (!inSleep) {
    res = sd_bus_process(bus, nullptr);
    if (res < 0) {
      std::cerr << "Error reading: " << strerror(-res) << "\n";
      break;
    }

    if (res > 0) {
      continue;
    }

    res = sd_bus_wait(bus, -1);
    if (res < 0) {
      std::cerr << "Error reading: " << strerror(-res) << "\n";
      break;
    }
  }

  sd_bus_error_free(&error);
  sd_bus_unref(bus);

  return true;
#else
  return true;
#endif
}

bool
powerOff() {
#ifdef __linux__
  sd_bus* bus = nullptr;
  int res = sd_bus_open_system(&bus);
  if (res < 0) {
    std::cerr << "Error opening system bus: " << strerror(-res) << "\n";
    return false;
  }

  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message* reply = nullptr;
  res = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           "PowerOff",
                           &error,
                           &reply,
                           "b",
                           /* interactive */ 0);
  if (res < 0) {
    std::cerr << "Error powering off: " << strerror(-res) << "\n";
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return false;
  }

  sd_bus_message_unref(reply);
  sd_bus_error_free(&error);
  sd_bus_unref(bus);
  return true;
#else
  return true;
#endif
}

unistdpp::Result<unistdpp::FD>
getInhibitLock() {
#ifdef __linux__
  sd_bus* bus = nullptr;
  int res = sd_bus_open_system(&bus);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }

  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message* reply = nullptr;
  int fd = -1;
  res = sd_bus_call_method(bus,
                           "org.freedesktop.login1",
                           "/org/freedesktop/login1",
                           "org.freedesktop.login1.Manager",
                           "Inhibit",
                           &error,
                           &reply,
                           "ssss",
                           "idle",
                           "rocket",
                           "Inhibit idle from rocket",
                           "block");
  if (res < 0) {
    sd_bus_unref(bus);
    return tl::unexpected(std::errc(-res));
  }

  res = sd_bus_message_read(reply, "h", &fd);
  if (res < 0) {
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    sd_bus_unref(bus);

    return tl::unexpected(std::errc(-res));
  }

  auto resFD = unistdpp::FD(dup(fd));
  TRY(unistdpp::fcntl<int>(resFD, F_SETFD, FD_CLOEXEC));

  sd_bus_message_unref(reply);
  sd_bus_error_free(&error);
  sd_bus_unref(bus);

  return resFD;
#endif
}

} // namespace systemdpp
