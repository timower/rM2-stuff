#pragma once

#include <unistdpp/socket.h>

#include <optional>
#include <sys/socket.h>
#include <sys/un.h>

constexpr std::string_view default_sock_addr = "/var/run/rm2fb.sock";

struct ControlSocket {
  unistdpp::FD sock;

  unistdpp::Result<void> init(const char* addr);
  unistdpp::Result<void> connect(const char* addr) const;

  unistdpp::Result<unistdpp::FD> accept() const;
  unistdpp::Result<void> listen(int n) const;

  template<typename T,
           typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  [[nodiscard]] unistdpp::Result<std::pair<T, unistdpp::Address>> recvfrom()
    const {
    T t{};
    unistdpp::Address addr;
    return unistdpp::recvfrom(sock, &t, sizeof(T), 0, &addr).map([&](auto _) {
      return std::pair{ t, addr };
    });
  }

  template<typename T,
           typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  [[nodiscard]] unistdpp::Result<ssize_t> sendto(
    const T& t,
    std::optional<unistdpp::Address> dest = std::nullopt) const {
    return unistdpp::sendto(
      sock, &t, sizeof(T), 0, dest.has_value() ? &*dest : nullptr);
  }
};
