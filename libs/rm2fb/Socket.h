#pragma once

#include <cstdio>
#include <optional>
#include <sys/socket.h>
#include <sys/un.h>
#include <tuple>

#define DEFAULT_SOCK_ADDR "/var/run/rm2fb.sock"

struct Socket {
  struct Address {
    sockaddr_un addr;
    socklen_t len = sizeof(sockaddr_un);
  };

  int sock = -1;

  Socket();
  ~Socket();

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  Socket(Socket&& other);
  Socket& operator=(Socket&& other);

  bool bind(const char* addr);
  bool connect(const char* addr);

  template<typename T>
  std::pair<std::optional<T>, Address> recvfrom() {
    T t;
    Address addr;

    int res = ::recvfrom(sock,
                         &t,
                         sizeof(T),
                         0,
                         reinterpret_cast<sockaddr*>(&addr.addr),
                         &addr.len);

    if (res != sizeof(T)) {
      perror("Recvfrom");
      return { std::nullopt, addr };
    }

    return { t, addr };
  }

  template<typename T>
  bool sendto(const T& t, std::optional<Address> dest = std::nullopt) {
    int res = ::sendto(
      sock,
      &t,
      sizeof(T),
      0,
      reinterpret_cast<sockaddr*>(dest.has_value() ? &dest->addr : nullptr),
      dest.has_value() ? dest->len : 0);

    if (res != sizeof(T)) {
      perror("Sendto");
      return false;
    }

    return true;
  }
};
