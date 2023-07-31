#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Socket.h"
#include "common.h"

int
main() {

  Socket sock;

  unlink("test-socket");
  if (!sock.bind("test-socket")) {
    return EXIT_FAILURE;
  }

  while (true) {
    auto [buf, addr] = sock.recvfrom<Buf>();

    if (buf) {
      std::cout << "Got: " << buf->str << std::endl;
    }
    const char* name =
      addr.addr.sun_path[0] == 0 ? &addr.addr.sun_path[1] : addr.addr.sun_path;
    std::cout << "From: " << name << std::endl;

    Buf b;
    strcpy(b.str, "OK\n");
    sock.sendto(b, addr);
  }

  return 0;
}
