#include "CommandSocket.h"

#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
constexpr auto server_path = "/var/run/rocket.sock";
}

bool
CommandSocket::alreadyRunning() {
  return access(server_path, F_OK) == 0;
}

bool
CommandSocket::send(std::string_view cmd) {
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("Error opening socket");
    return false;
  }

  struct sockaddr_un server;
  server.sun_family = AF_UNIX;
  strcpy(server.sun_path, server_path);

  if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_un)) !=
      0) {
    perror("Error connecting to server");
    return false;
  }

  size_t written = write(sock, cmd.data(), cmd.size());
  if (written != cmd.size()) {
    perror("Error writing");
    return false;
  }

  std::string buf(512, '\0');
  int size = read(sock, buf.data(), buf.size());
  if (size <= 0) {
    perror("Error reading");
    return false;
  }

  buf.resize(size);
  std::cout << buf << std::endl;
  return true;
}

bool
CommandSocket::init() {
  serverSock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (serverSock < 0) {
    perror("opening stream socket");
    return false;
  }

  struct sockaddr_un server;
  server.sun_family = AF_UNIX;
  strcpy(server.sun_path, server_path);

  unlink(server_path);
  if (bind(serverSock, (struct sockaddr*)&server, sizeof(struct sockaddr_un))) {
    perror("binding stream socket");
    return false;
  }

  printf("Socket has name %s\n", server.sun_path);
  if (0 != listen(serverSock, /* backlog */ 5)) {
    perror("Error listening");
    return false;
  }

  return true;
}

CommandSocket::~CommandSocket() {
  if (client != -1) {
    close(client);
  }
  if (serverSock != -1) {
    close(serverSock);
    unlink(server_path);
  }
}

int
CommandSocket::getFd() {
  if (client != -1) {
    return client;
  }

  return serverSock;
}

void
CommandSocket::process() {
  if (client != -1) {
    std::string buf(512, '\0');
    auto size = read(client, buf.data(), buf.size());
    if (size < 0) {
      perror("Error reading from client");
      close(client);
      client = -1;
      return;
    }

    auto nl = buf.find('\n');
    if (nl != std::string::npos) {
      size = nl;
    }

    auto response = callback(std::string_view(buf.data(), size));
    size_t written = write(client, response.data(), response.size());
    if (written != response.size()) {
      perror("Error writing to client");
    }

    close(client);
    client = -1;
    return;
  }

  client = accept(serverSock, nullptr, nullptr);
  if (client == -1) {
    perror("Error accepting client");
  }
}
