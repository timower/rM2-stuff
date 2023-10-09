#include "ControlSocket.h"

#include <cstdio>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace unistdpp;

Result<void>
ControlSocket::init(const char* addr) {
  sock = TRY(unistdpp::socket(AF_UNIX, SOCK_DGRAM, 0));
  return unistdpp::bind(sock, Address::fromUnixPath(addr));
}

Result<void>
ControlSocket::connect(const char* addr) {
  return unistdpp::connect(sock, Address::fromUnixPath(addr));
}
