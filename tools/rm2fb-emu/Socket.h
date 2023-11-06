#pragma once

#include <unistdpp/socket.h>

inline unistdpp::Result<unistdpp::FD>
getClientSock(const char* host, int port) {
  auto addr = TRY(unistdpp::Address::fromHostPort(host, port));
  auto sock = TRY(unistdpp::socket(AF_INET, SOCK_STREAM, 0));
  TRY(unistdpp::connect(sock, addr));
  return sock;
}
