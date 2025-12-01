#include "rm2fb/ControlSocket.h"

#include <memory>
#include <unistdpp/error.h>

#include <cstdio>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
constexpr auto max_clients = 64;

enum class MsgType {
  GetClients,
  GetFB,
  SwitchTo,
  SetLauncher,
};

struct Request {
  MsgType type;
  pid_t pid;
};

struct ClientResponse {
  int nClients;
  ControlInterface::Client clients[];
};

auto
sendMsg(const unistdpp::FD& fd, const Request& req) {
  static const auto server_addr =
    unistdpp::Address::fromUnixPath(control_sock_addr.data());
  return unistdpp::sendto(fd, &req, sizeof(Request), 0, &server_addr);
}

unistdpp::Result<void>
readBoolResponse(const unistdpp::FD& fd) {

  bool res = false;
  TRY(unistdpp::recvfrom(fd, &res, sizeof(res), 0, nullptr));
  if (!res) {
    return tl::unexpected(std::errc::bad_file_descriptor);
  }
  return {};
}

} // namespace

unistdpp::Result<void>
ControlServer::maybeInit() {
  if (sock.isValid()) {
    std::cerr << "Using control socket from systemd\n";
    return {};
  }
  const char* socketAddr = control_sock_addr.data();
  if (unlink(socketAddr) != 0) {
    perror("Socket unlink");
  }

  sock = TRY(unistdpp::socket(AF_UNIX, SOCK_DGRAM, 0));
  return unistdpp::bind(sock, unistdpp::Address::fromUnixPath(socketAddr));
}

unistdpp::Result<void>
ControlServer::handleMsg() {
  Request req;
  unistdpp::Address addr;
  auto size = TRY(unistdpp::recvfrom(sock, &req, sizeof(req), 0, &addr));
  if (size < long(sizeof(MsgType)) ||
      (req.type != MsgType::GetClients && size < long(sizeof(Request)))) {
    return tl::unexpected(std::errc::bad_message);
  }

  unistdpp::Result<void> res;
  std::cerr << "Got request: " << int(req.type) << "\n";

  switch (req.type) {
    case MsgType::GetClients: {
      auto clients = TRY(iface.getClients());
      if (clients.size() > max_clients) {
        clients.resize(max_clients);
      }

      const auto bufsize = sizeof(ClientResponse) +
                           (clients.size() * sizeof(ControlInterface::Client));

      auto buf = std::make_unique<char[]>(bufsize);

      auto* ptr = (ClientResponse*)buf.get();
      ptr->nClients = clients.size();
      memcpy(&ptr->clients[0],
             clients.data(),
             clients.size() * sizeof(ControlInterface::Client));

      TRY(unistdpp::sendto(sock, ptr, bufsize, 0, &addr));

      return {};
    }

    case MsgType::GetFB:
      break;

    case MsgType::SwitchTo:
      res = iface.switchTo(req.pid);
      break;
    case MsgType::SetLauncher:
      res = iface.setLauncher(req.pid);
      break;
  }

  bool response = res.has_value();
  TRY(unistdpp::sendto(sock, &response, sizeof(response), 0, &addr));

  return res;
}

unistdpp::Result<void>
ControlClient::init() {
  sock = TRY(unistdpp::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0));

  return unistdpp::bind(sock, unistdpp::Address::fromUnixPath(nullptr))
    .and_then([this] {
      return unistdpp::connect(
        sock, unistdpp::Address::fromUnixPath(control_sock_addr.data()));
    })
    .or_else([this](auto err) { sock.close(); });
}

unistdpp::Result<std::vector<ControlInterface::Client>>
ControlClient::getClients() {
  Request req{
    .type = MsgType::GetClients,
    .pid = 0,
  };
  TRY(sendMsg(sock, req));

  const auto bufsize =
    sizeof(ClientResponse) + (max_clients * sizeof(ControlInterface::Client));
  auto buf = std::make_unique<char[]>(bufsize);
  auto size = TRY(unistdpp::recvfrom(sock, buf.get(), bufsize, 0, nullptr));
  if (size < long(sizeof(ClientResponse))) {
    std::cerr << "Got " << size << " response to getClients\n";
    return tl::unexpected(std::errc::bad_message);
  }

  auto* ptr = (ClientResponse*)buf.get();
  std::vector<ControlInterface::Client> res;
  res.reserve(ptr->nClients);
  std::copy(
    &ptr->clients[0], &ptr->clients[ptr->nClients], std::back_inserter(res));

  return res;
}

unistdpp::Result<unistdpp::FD>
ControlClient::getFramebuffer(pid_t pid) {
  return tl::unexpected(std::errc::not_supported);
}

unistdpp::Result<void>
ControlClient::switchTo(pid_t pid) {
  Request req{
    .type = MsgType::SwitchTo,
    .pid = pid,
  };
  TRY(sendMsg(sock, req));
  return readBoolResponse(sock);
}

unistdpp::Result<void>
ControlClient::setLauncher(pid_t pid) {
  Request req{
    .type = MsgType::SetLauncher,
    .pid = pid,
  };
  TRY(sendMsg(sock, req));
  return readBoolResponse(sock);
}
