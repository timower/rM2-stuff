#include "ControlSocket.h"

#include <cstdio>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace unistdpp;

Result<void>
ControlSocket::init(const char* addr) {
  sock = TRY(unistdpp::socket(AF_UNIX, SOCK_STREAM, 0));
  return unistdpp::bind(sock, Address::fromUnixPath(addr));
}

Result<void>
ControlSocket::connect(const char* addr) const {
  timeval tv{ .tv_sec = 5, .tv_usec = 0 };
  TRY(unistdpp::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));
  return unistdpp::connect(sock, Address::fromUnixPath(addr));
}

Result<void>
ControlSocket::listen(int n) const {
  return unistdpp::listen(sock, n);
}

unistdpp::Result<unistdpp::FD>
ControlSocket::accept() const {
  return unistdpp::accept(sock, nullptr, nullptr);
}
