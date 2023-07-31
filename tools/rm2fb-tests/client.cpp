#include <iostream>
#include <sys/socket.h>

#include "Socket.h"
#include "common.h"

int
main() {

  Socket sock;
  if (!sock.bind(nullptr)) {
    return EXIT_FAILURE;
  }

  if (!sock.connect("test-socket")) {
    return EXIT_FAILURE;
  }

  Buf buf;
  strcpy(buf.str, "Hello!");
  sock.sendto(buf);

  auto [reply, _] = sock.recvfrom<Buf>();
  if (reply) {
    std::cout << "Reply: " << reply->str << std::endl;
  }

  return 0;
}
