#pragma once

#include "unistdpp/socket.h"
#include <iomanip>
#include <unistdpp/unistdpp.h>

#include <cstdint>
#include <iostream>
#include <variant>

constexpr std::string_view default_sock_addr = "/var/run/rm2fb.sock";

struct UpdateParams {
  static constexpr auto ioctl_waveform_flag = 0xf000;

  int y1;
  int x1;
  int y2;
  int x2;

  int flags;
  int waveform;

  float temperatureOverride;
  int extraMode;
};

constexpr auto update_message_size = 8 * 4;
static_assert(sizeof(UpdateParams) == update_message_size,
              "Params has wrong size?");

inline std::ostream&
operator<<(std::ostream& stream, const UpdateParams& msg) {
  // clang-format off
  return stream << "{ { "
                << std::setw(4) << msg.x1 << ", " << std::setw(4) << msg.y1
                << "; "
                << std::setw(4) << msg.x2 << ", " << std::setw(4) << msg.y2
                << " },"
                << std::hex
                << " wave: " << msg.waveform
                << " flags: " << msg.flags
                << std::dec
                << " temp: " << msg.temperatureOverride
                << " mode: " << msg.extraMode << " }";
  // clang-format on
}

struct Input {
  int32_t x = 0;
  int32_t y = 0;
  enum Action { Move, Down, Up } type = Move;
  bool touch = false; // True for touch, false for pen
};

static_assert(sizeof(Input) == 4 * 4, "Input message has unexpected size");

struct GetUpdate {};

struct PowerButton {
  bool down;
};

using ClientMsg = std::variant<Input, GetUpdate, PowerButton>;

template<typename... T>
unistdpp::Result<void>
sendMessage(const unistdpp::FD& fd, const std::variant<T...>& msg) {
  TRY(fd.writeAll<int32_t>(msg.index()));
  return std::visit([&](const auto& msg) { return fd.writeAll(msg); }, msg);
}

namespace details {

template<typename Variant, auto idx = 0>
unistdpp::Result<Variant>
read(const unistdpp::FD& fd, int32_t index) {
  if constexpr (idx >= std::variant_size_v<Variant>) {
    (void)index;
    return tl::unexpected(std::errc::bad_message);
  } else {
    if (idx == index) {
      using T = std::variant_alternative_t<idx, Variant>;
      return fd.readAll<T>();
    }
    return read<Variant, idx + 1>(fd, index);
  }
}

template<typename Variant, auto idx = 0>
unistdpp::Result<Variant>
parse(const char* buf, int32_t index) {
  if constexpr (idx >= std::variant_size_v<Variant>) {
    (void)index;
    return tl::unexpected(std::errc::bad_message);
  } else {
    if (idx == index) {
      using T = std::variant_alternative_t<idx, Variant>;
      T msg;
      memcpy(&msg, buf, sizeof(T));
    }
    return parse<Variant, idx + 1>(buf, index);
  }
}

} // namespace details

template<typename Variant>
unistdpp::Result<Variant>
recvMessage(const unistdpp::FD& fd) {
  auto idx = TRY(fd.readAll<int32_t>());
  return details::read<Variant>(fd, idx);
}

template<typename Variant>
unistdpp::Result<std::pair<unistdpp::Address, Variant>>
recvMessageFrom(const unistdpp::FD& fd) {
  unistdpp::Address addr;
  char buf[sizeof(Variant) + sizeof(int32_t)];
  auto size = TRY(unistdpp::recvfrom(fd, &buf, sizeof(buf), 0, &addr));
  if (size < 0 || static_cast<unsigned>(size) < sizeof(int32_t)) {
    return tl::unexpected(std::errc::message_size);
  }

  int32_t idx;
  memcpy(&idx, buf, sizeof(idx));

  auto val = TRY(details::parse<Variant>(&buf[sizeof(idx)], idx));
  return std::pair{ addr, val };
}
