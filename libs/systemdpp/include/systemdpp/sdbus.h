#pragma once

#include <unistdpp/unistdpp.h>

#include <optional>

struct sd_bus;
struct sd_bus_message;

namespace systemdpp {

// A single sd-bus message, e.g. a method reply or a signal picked up by
// Bus::process(). Thin wrapper around sd_bus_message*.
class Message {
public:
  explicit Message(sd_bus_message* msg);
  Message(Message&& other) noexcept;
  Message& operator=(Message&& other) noexcept;
  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;
  ~Message();

  bool isSignal(const char* interface, const char* member) const;

  unistdpp::Result<bool> readBool() const;
  unistdpp::Result<unistdpp::FD> readFD() const;

private:
  sd_bus_message* msg = nullptr;
};

// Thin RAII wrapper around a system D-Bus connection - wraps sd-bus's own
// API rather than any particular service's, much like unistdpp wraps POSIX.
class Bus {
public:
  static unistdpp::Result<Bus> openSystem();

  Bus(Bus&& other) noexcept;
  Bus& operator=(Bus&& other) noexcept;
  Bus(const Bus&) = delete;
  Bus& operator=(const Bus&) = delete;
  ~Bus();

  // Pollable fd; becomes readable when there's a message to process().
  int fd() const;

  // Thin wrapper around sd_bus_call_method: `signature`/`args` are
  // forwarded to it as-is, e.g. callMethod(..., "ssss", "a", "b", "c", "d").
  template<typename... Args>
  unistdpp::Result<Message> callMethod(const char* destination,
                                       const char* path,
                                       const char* interface,
                                       const char* member,
                                       const char* signature,
                                       Args... args) const;

  // Fire-and-forget version of callMethod(): sends the call and returns
  // without waiting for (or tracking) the reply, unlike sd_bus_call_method
  // this can't block our own process()/dispatch loop on ourselves - needed
  // for calls (e.g. login1's Suspend) whose reply only arrives after we've
  // reacted to a signal that same call triggers.
  template<typename... Args>
  unistdpp::Result<void> callMethodAsync(const char* destination,
                                         const char* path,
                                         const char* interface,
                                         const char* member,
                                         const char* signature,
                                         Args... args) const;

  // Thin wrapper around sd_bus_match_signal with no callback registered -
  // this only tells the bus daemon to route matching signals to us;
  // process() is what actually surfaces them.
  unistdpp::Result<void> matchSignal(const char* sender,
                                     const char* path,
                                     const char* interface,
                                     const char* member);

  // Thin wrapper around sd_bus_process. `hasMore` mirrors its return value
  // (whether calling again may produce more work); `message` is set if
  // this call surfaced an unhandled incoming message.
  struct ProcessResult {
    bool hasMore;
    std::optional<Message> message;
  };
  unistdpp::Result<ProcessResult> process();

private:
  explicit Bus(sd_bus* bus) : bus(bus) {}
  sd_bus* bus = nullptr;
};

} // namespace systemdpp
