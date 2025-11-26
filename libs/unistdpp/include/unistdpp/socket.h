#pragma once

#include "traits.h"
#include "unistdpp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <string_view>
#include <tuple>
#include <variant>

namespace unistdpp {

struct Address {
  static Address fromUnixPath(const char* path);
  static Address fromHostPort(int32_t host, int port);
  static Result<Address> fromHostPort(std::string_view host, int port);

  [[nodiscard]] sockaddr* ptr() noexcept {
    // NOLINTNEXTLINE
    return reinterpret_cast<sockaddr*>(&storage);
  }

  [[nodiscard]] const sockaddr* ptr() const noexcept {
    // NOLINTNEXTLINE
    return reinterpret_cast<const sockaddr*>(&storage);
  }

  [[nodiscard]] sa_family_t type() const noexcept { return ptr()->sa_family; }

  [[nodiscard]] socklen_t size() const noexcept { return addressSize; }
  [[nodiscard]] socklen_t* sizePtr() noexcept { return &addressSize; }

private:
  std::aligned_union_t<0, sockaddr_un, sockaddr_in> storage = {};
  socklen_t addressSize = sizeof(storage);
};

template<>
struct WrapperTraits<const Address&> {
  static std::tuple<const sockaddr*, socklen_t> arg(const Address& addr) {
    return { addr.ptr(), addr.size() };
  }
};

template<>
struct WrapperTraits<const Address*> {
  static std::tuple<const sockaddr*, socklen_t> arg(const Address* addr) {
    return { addr != nullptr ? addr->ptr() : nullptr,
             addr != nullptr ? addr->size() : 0 };
  }
};

template<>
struct WrapperTraits<Address*> {
  static std::tuple<sockaddr*, socklen_t*> arg(Address* addr) {
    return { addr != nullptr ? addr->ptr() : nullptr,
             addr != nullptr ? addr->sizePtr() : nullptr };
  }
};

// int
// socket(int domain, int type, int protocol);
constexpr auto socket = FnWrapper<::socket, Result<FD>(int, int, int)>{};

// int
// setsockopt(int socket, int level, int option_name,
//            const void *option_value, socklen_t option_len);
constexpr auto setsockopt =
  FnWrapper<::setsockopt,
            Result<void>(const FD&, int, int, const void*, socklen_t)>{};

// int getsockopt( int sockfd, int level, int optname, void* optval, socklen_t
// *restrict optlen);
constexpr auto getsockopt =
  FnWrapper<::getsockopt,
            Result<void>(const FD&, int, int, void*, socklen_t*)>{};

// int
// bind(int socket, const struct sockaddr *address, socklen_t address_len);
constexpr auto bind_raw =
  FnWrapper<::bind, Result<void>(const FD& fd, const sockaddr*, socklen_t)>{};
constexpr auto bind =
  FnWrapper<::bind, Result<void>(const FD& fd, const Address&)>{};

// int
// listen(int socket, int backlog);
constexpr auto listen = FnWrapper<::listen, Result<void>(const FD&, int)>{};

// int
// accept(int socket, struct sockaddr *restrict address,
//        socklen_t *restrict address_len);
constexpr auto accept =
  FnWrapper<::accept, Result<FD>(const FD&, sockaddr*, socklen_t*)>{};

// int connect(int socket, const struct sockaddr *address,
//             socklen_t address_len);
constexpr auto connect_raw =
  FnWrapper<::connect, Result<void>(const FD&, const sockaddr*, socklen_t)>{};
constexpr auto connect =
  FnWrapper<::connect, Result<void>(const FD&, const Address&)>{};

// ssize_t sendto(int socket, const void *message, size_t length,
//                int flags, const struct sockaddr *dest_addr,
//                socklen_t dest_len);
constexpr auto sendto = FnWrapper<
  ::sendto,
  Result<ssize_t>(const FD&, const void*, size_t, int, const Address*)>{};

// ssize_t recvfrom(int socket, void *restrict buffer, size_t length,
//                  int flags, struct sockaddr *restrict address,
//                  socklen_t *restrict address_len);
constexpr auto recvfrom =
  FnWrapper<::recvfrom,
            Result<ssize_t>(const FD&, void*, size_t, int, Address*)>{};

} // namespace unistdpp
