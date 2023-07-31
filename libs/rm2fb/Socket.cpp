#include "Socket.h"

#include <cstdio>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
sockaddr_un
getAddress(const char* path) {
  sockaddr_un sockName;
  memset(&sockName, 0, sizeof(sockaddr_un));
  sockName.sun_family = AF_UNIX;

  if (path != nullptr) {
    strncpy(sockName.sun_path, path, sizeof(sockName.sun_path) - 1);
  }

  return sockName;
}
} // namespace

Socket::Socket() {
  sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (sock == -1) {
    perror("Socket");
    return;
  }
}

Socket::~Socket() {
  if (sock != -1) {
    close(sock);
  }
}

Socket::Socket(Socket&& other) : sock(other.sock) {
  other.sock = -1;
}

Socket&
Socket::operator=(Socket&& other) {
  // TODO: close
  sock = other.sock;
  other.sock = -1;
  return *this;
}

bool
Socket::bind(const char* addr) {
  auto sockName = getAddress(addr);

  // Use autobind if addr is nullptr.
  int ret = ::bind(sock,
                   reinterpret_cast<sockaddr*>(&sockName),
                   addr == nullptr ? sizeof(sa_family_t) : sizeof(sockName));
  if (ret == -1) {
    perror("Bind");
    return false;
  }

  return true;
}

bool
Socket::connect(const char* addr) {
  auto sockName = getAddress(addr);
  int ret =
    ::connect(sock, reinterpret_cast<sockaddr*>(&sockName), sizeof(sockName));
  if (ret == -1) {
    perror("Connect");
    return false;
  }
  return true;
}
