#include "Messages.h"

// rm2fb
#include <ControlSocket.h>
#include <ImageHook.h>
#include <Message.h>
#include <uinput.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <vector>

#include <unistdpp/poll.h>
#include <unistdpp/socket.h>
#include <unistdpp/unistdpp.h>

#include <libevdev/libevdev-uinput.h>

using namespace unistdpp;

namespace {
std::atomic_bool running = true;

void
onSigint(int num) {
  running = false;
}

void
setupExitHandler() {
  struct sigaction action;

  action.sa_handler = onSigint;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("Sigaction");
    exit(EXIT_FAILURE);
  }
}

bool
doUpdate(const FD& fd, const UpdateParams& params) {
  if (auto res = fd.writeAll(params); !res) {
    std::cerr << "Error writing: " << toString(res.error()) << "\n";
    exit(EXIT_FAILURE);
  }

  int width = params.x2 - params.x1 + 1;
  int height = params.y2 - params.y1 + 1;
  int size = width * height;
  std::vector<uint16_t> buffer(size);

  for (int row = 0; row < height; row++) {
    int fbRow = row + params.y1;

    memcpy(buffer.data() + row * width,
           fb.mem + fbRow * fb_width + params.x1,
           width * sizeof(uint16_t));
  }

  int writeSize = size * sizeof(uint16_t);
  auto res = fd.writeAll(buffer.data(), writeSize);
  if (!res) {
    std::cerr << "Error writing: " << toString(res.error()) << "\n";
    exit(EXIT_FAILURE);
  }

  return true;
}

// Starts a tcp server and waits for a client to connect.
Result<FD>
getTcpSocket(int port) {
  auto listenfd = TRY(unistdpp::socket(AF_INET, SOCK_STREAM, 0));

  // lose the pesky "Address already in use" error message
  int yes = 1;
  TRY(unistdpp::setsockopt(
    listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

  TRY(unistdpp::bind(listenfd, Address::fromHostPort(INADDR_ANY, port)));
  TRY(unistdpp::listen(listenfd, 1));

  // return unistdpp::accept(listenfd, nullptr, nullptr);
  return listenfd;
}
} // namespace

int
main() {
  setupExitHandler();
  std::cout << "Mem: " << fb.mem << "\n";

  auto buttonDev = makeButtonDevice();
  auto uinputDev = makeWacomDevice();
  auto touchDev = makeTouchDevice();

  const char* socketAddr = default_sock_addr.data();

  // Setup server socket.
  if (unlink(socketAddr) != 0) {
    perror("Socket unlink");
  }

  ControlSocket serverSock;
  if (!serverSock.init(socketAddr)) {
    return EXIT_FAILURE;
  }

  // Open shared memory
  if (fb.mem == nullptr) {
    return EXIT_FAILURE;
  }
  memset(fb.mem, UINT8_MAX, fb_size);

  constexpr auto port = 8888;
  std::cout << "Listening for TCP connections on: " << port << "\n";
  auto tcpFd = getTcpSocket(port);
  if (!tcpFd) {
    std::cerr << "Unable to get TCP connection\n";
    return EXIT_FAILURE;
  }

  constexpr auto tcp_poll_idx = 0;
  constexpr auto server_poll_idx = 1;
  constexpr auto fixed_poll_fds = server_poll_idx + 1;

  std::vector<unistdpp::FD> clientSocks;

  std::cout << "SWTCON :p!\n";
  while (running) {
    std::vector<pollfd> pollfds(fixed_poll_fds);
    pollfds.reserve(clientSocks.size() + fixed_poll_fds);
    pollfds[tcp_poll_idx] = waitFor(*tcpFd, Wait::Read);
    pollfds[server_poll_idx] = waitFor(serverSock.sock, Wait::Read);
    std::transform(
      clientSocks.begin(),
      clientSocks.end(),
      std::back_inserter(pollfds),
      [](const auto& client) { return waitFor(client, Wait::Read); });

    auto res = unistdpp::poll(pollfds);
    if (!res) {
      std::cerr << "Poll error: " << toString(res.error()) << "\n";
      break;
    }

    if (canRead(pollfds[tcp_poll_idx])) {
      auto clientOrErr = unistdpp::accept(*tcpFd, nullptr, nullptr);
      if (clientOrErr) {
        std::cout << "Got new client!\n";
        clientSocks.emplace_back(std::move(*clientOrErr));
        pollfds.push_back(waitFor(clientSocks.back(), Wait::Read));
      } else if (!clientOrErr) {
        std::cerr << "Client accept errror: " << toString(clientOrErr.error())
                  << "\n";
      }
    }

    if (canRead(pollfds[server_poll_idx])) {
      auto result =
        serverSock.recvfrom<UpdateParams>().and_then([&](auto result) {
          auto [msg, addr] = result;

          auto res = false;
          for (const auto& clientSock : clientSocks) {
            res |= doUpdate(clientSock, msg);
          }

          // Don't log Stroke updates
          if (msg.flags != 4) {
            std::cerr << "UPDATE " << msg << ": " << res << "\n";
          }

          return serverSock.sendto(res, addr);
        });
      if (!result) {
        std::cerr << "Error update: " << toString(result.error()) << "\n";
      }
    }

    for (size_t idx = fixed_poll_fds; idx < pollfds.size(); idx++) {
      if (!canRead(pollfds[idx])) {
        continue;
      }

      auto& clientSock = clientSocks[idx - fixed_poll_fds];
      clientSock.readAll<Input>()
        .map([&](auto input) {
          if (uinputDev != nullptr) {
            sendInput(input, *uinputDev);
          }
        })
        .or_else([&](auto err) {
          std::cerr << "Reading input: " << toString(err) << "\n";
          if (err == unistdpp::FD::eof_error) {
            clientSock.close();
          }
        });
    }

    // Remove closed clients
    clientSocks.erase(
      std::remove_if(clientSocks.begin(),
                     clientSocks.end(),
                     [](const auto& sock) { return !sock.isValid(); }),
      clientSocks.end());
  }

  return 0;
}
