#include "unistdpp/socket.h"

namespace unistdpp {
Address
Address::fromUnixPath(const char* path) {
  Address res;
  auto* addr = new (&res.storage) sockaddr_un();
  memset(addr, 0, sizeof(sockaddr_un));
  addr->sun_family = AF_UNIX;

  if (path != nullptr) {
    strncpy(addr->sun_path, path, sizeof(addr->sun_path) - 1);
    res.addressSize = sizeof(sockaddr_un);
  } else {
    res.addressSize = sizeof(sa_family_t);
  }

  return res;
}

Address
Address::fromHostPort(int32_t host, int port) {
  Address res;
  res.addressSize = sizeof(sockaddr_in);
  auto* addr = new (&res.storage) sockaddr_in();
  memset(addr, 0, sizeof(sockaddr_in));
  addr->sin_family = AF_INET;

  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(host);

  return res;
}

Result<Address>
Address::fromHostPort(std::string_view host, int port) {
  Address res;
  res.addressSize = sizeof(sockaddr_in);
  auto* addr = new (&res.storage) sockaddr_in();
  memset(addr, 0, sizeof(sockaddr_in));
  addr->sin_family = AF_INET;

  addr->sin_port = htons(port);

  int err = inet_pton(AF_INET, host.data(), &addr->sin_addr);
  if (err == 0) {
    return tl::unexpected(std::errc::invalid_argument);
  }
  if (err < 0) {
    return tl::unexpected(getErrno());
  }

  return res;
}

} // namespace unistdpp
