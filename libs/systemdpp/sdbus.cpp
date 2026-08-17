#include <systemdpp/sdbus.h>

#include <unistdpp/file.h>

#ifdef __linux__
#include <systemd/sd-bus.h>
#endif

namespace systemdpp {

Message::Message(sd_bus_message* msg) : msg(msg) {}

Message::Message(Message&& other) noexcept : msg(other.msg) {
  other.msg = nullptr;
}

Message&
Message::operator=(Message&& other) noexcept {
  if (this != &other) {
#ifdef __linux__
    if (msg != nullptr) {
      sd_bus_message_unref(msg);
    }
#endif
    msg = other.msg;
    other.msg = nullptr;
  }
  return *this;
}

Message::~Message() {
#ifdef __linux__
  if (msg != nullptr) {
    sd_bus_message_unref(msg);
  }
#endif
}

bool
Message::isSignal(const char* interface, const char* member) const {
#ifdef __linux__
  return sd_bus_message_is_signal(msg, interface, member) != 0;
#else
  return false;
#endif
}

unistdpp::Result<bool>
Message::readBool() const {
#ifdef __linux__
  int value = 0;
  int res = sd_bus_message_read(msg, "b", &value);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }
  return value != 0;
#else
  return tl::unexpected(std::errc::not_supported);
#endif
}

unistdpp::Result<unistdpp::FD>
Message::readFD() const {
#ifdef __linux__
  int fd = -1;
  int res = sd_bus_message_read(msg, "h", &fd);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }

  auto owned = unistdpp::FD(dup(fd));
  auto fcntlRes = unistdpp::fcntl<int>(owned, F_SETFD, FD_CLOEXEC);
  if (!fcntlRes) {
    return tl::unexpected(fcntlRes.error());
  }
  return owned;
#else
  return tl::unexpected(std::errc::not_supported);
#endif
}

unistdpp::Result<Bus>
Bus::openSystem() {
#ifdef __linux__
  sd_bus* bus = nullptr;
  int res = sd_bus_open_system(&bus);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }
  return Bus(bus);
#else
  return tl::unexpected(std::errc::not_supported);
#endif
}

Bus::Bus(Bus&& other) noexcept : bus(other.bus) {
  other.bus = nullptr;
}

Bus&
Bus::operator=(Bus&& other) noexcept {
  if (this != &other) {
#ifdef __linux__
    if (bus != nullptr) {
      sd_bus_unref(bus);
    }
#endif
    bus = other.bus;
    other.bus = nullptr;
  }
  return *this;
}

Bus::~Bus() {
#ifdef __linux__
  if (bus != nullptr) {
    sd_bus_unref(bus);
  }
#endif
}

int
Bus::fd() const {
#ifdef __linux__
  return sd_bus_get_fd(bus);
#else
  return -1;
#endif
}

template<typename... Args>
unistdpp::Result<Message>
Bus::callMethod(const char* destination,
                const char* path,
                const char* interface,
                const char* member,
                const char* signature,
                Args... args) const {
#ifdef __linux__
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message* reply = nullptr;
  int res = sd_bus_call_method(bus,
                               destination,
                               path,
                               interface,
                               member,
                               &error,
                               &reply,
                               signature,
                               args...);
  sd_bus_error_free(&error);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }
  return Message(reply);
#else
  return tl::unexpected(std::errc::not_supported);
#endif
}

// Explicit instantiations for the call shapes actually needed by callers.
template unistdpp::Result<Message>
Bus::callMethod(const char*,
                const char*,
                const char*,
                const char*,
                const char*,
                int) const;
template unistdpp::Result<Message>
Bus::callMethod(const char*,
                const char*,
                const char*,
                const char*,
                const char*,
                const char*) const;
template unistdpp::Result<Message>
Bus::callMethod(const char*,
                const char*,
                const char*,
                const char*,
                const char*,
                const char*,
                const char*,
                const char*,
                const char*) const;

template<typename... Args>
unistdpp::Result<void>
Bus::callMethodAsync(const char* destination,
                     const char* path,
                     const char* interface,
                     const char* member,
                     const char* signature,
                     Args... args) const {
#ifdef __linux__
  // Passing null for both the callback and the ret_slot makes this a
  // "floating" async call - sd-bus owns and frees the pending-call state
  // itself once the reply (which we don't care about) arrives.
  int res = sd_bus_call_method_async(bus,
                                     nullptr,
                                     destination,
                                     path,
                                     interface,
                                     member,
                                     nullptr,
                                     nullptr,
                                     signature,
                                     args...);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }
  return {};
#else
  return tl::unexpected(std::errc::not_supported);
#endif
}

template unistdpp::Result<void>
Bus::callMethodAsync(const char*,
                     const char*,
                     const char*,
                     const char*,
                     const char*,
                     int) const;

unistdpp::Result<void>
Bus::matchSignal(const char* sender,
                 const char* path,
                 const char* interface,
                 const char* member) {
#ifdef __linux__
  int res = sd_bus_match_signal(
    bus, nullptr, sender, path, interface, member, nullptr, nullptr);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }
  return {};
#else
  return tl::unexpected(std::errc::not_supported);
#endif
}

unistdpp::Result<Bus::ProcessResult>
Bus::process() {
#ifdef __linux__
  sd_bus_message* msg = nullptr;
  int res = sd_bus_process(bus, &msg);
  if (res < 0) {
    return tl::unexpected(std::errc(-res));
  }

  ProcessResult result{ .hasMore = res > 0 };
  if (msg != nullptr) {
    result.message = Message(msg);
  }
  return result;
#else
  return tl::unexpected(std::errc::not_supported);
#endif
}

} // namespace systemdpp
