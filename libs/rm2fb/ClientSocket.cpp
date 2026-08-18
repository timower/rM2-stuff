// Update-sending code shared by every rm2fb client variant (Client.cpp,
// ClientSwtcon.cpp) - kept in its own translation unit so
// rm2fb_client_swtcon can pull it in without depending on the rest of
// Client.cpp (see ClientSwtcon.cpp's top comment).

#include "Client.h"

#include "rm2fb/Message.h"
#include "rm2fb/SharedBuffer.h"

#include "unistdpp/error.h"
#include "unistdpp/socket.h"

#include <cerrno>
#include <cstring>
#include <string_view>

namespace {

std::mutex&
controlSocketMutex() {
  static std::mutex m;
  return m;
}

// The actual lazily-connected fd - private to this file, only ever
// touched through ClientSocket, which holds the lock across its use.
unistdpp::FD&
rawControlSocket() {
  static unistdpp::FD res;
  if (!res.isValid()) {
    res = unistdpp::fatalOnError(
      unistdpp::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));

    unistdpp::bind(res, unistdpp::Address::fromUnixPath(nullptr))
      .and_then([] {
        return unistdpp::connect(
          res, unistdpp::Address::fromUnixPath(default_sock_addr.data()));
      })
      .or_else([](auto err) {
        std::cerr << "Failed connecting to rm2fb: " << unistdpp::to_string(err)
                  << "\n";
        res.close();
      });
  }
  return res;
}

} // namespace

ClientSocket::ClientSocket()
  : lock(controlSocketMutex()), sock(rawControlSocket()) {}

unistdpp::Result<void>
ClientSocket::init(bool ownSwtcon, FbFormat format, Buffer& fb) {
  if (!sock.isValid()) {
    return tl::unexpected(std::errc::not_connected);
  }

  return sendMessage(
           sock,
           UnixClientMsg{ Init{ .ownSwtcon = ownSwtcon, .format = format } })
    .and_then([&] { return fb.recv(sock); })
    .or_else([&](auto err) {
      std::cerr << "Error sending: " << unistdpp::to_string(err) << "\n";
      sock.close();
    });
}

unistdpp::Result<void>
ClientSocket::sendIdleUpdate(bool idle) {
  if (!sock.isValid()) {
    return tl::unexpected(std::errc::not_connected);
  }
  return sendMessage(sock, UnixClientMsg{ IdleUpdate{ idle } });
}

unistdpp::Result<int>
ClientSocket::openInputDevice(const char* pathname, int flags) {
  if (!sock.isValid()) {
    return tl::unexpected(std::errc::not_connected);
  }

  OpenInputDevice msg{};
  msg.flags = flags;
  strncpy(msg.path, pathname, sizeof(msg.path) - 1);

  return sendMessage(sock, UnixClientMsg{ msg })
    .and_then([&] { return sock.readAll<bool>(); })
    .and_then([&](bool ok) -> unistdpp::Result<int> {
      if (!ok) {
        return tl::unexpected(std::errc::no_such_device);
      }
      return unistdpp::recvFD(sock);
    });
}

bool
ClientSocket::sendUpdate(const UpdateParams& params) {
  if (!sock.isValid()) {
    return false;
  }

  return sendMessage(sock, UnixClientMsg{ params })
    .and_then([&]() { return sock.readAll<bool>(); })
    .or_else([&](auto err) {
      std::cerr << "Error sending: " << unistdpp::to_string(err) << "\n";
      sock.close();
    })
    .value_or(false);
}

bool
ClientSocket::sendUpdateBatch(const std::vector<UpdateParams>& updates) {
  if (!sock.isValid()) {
    return false;
  }

  const auto header =
    UpdateBatchHeader{ .count = static_cast<int32_t>(updates.size()) };

  return sendMessage(sock, UnixClientMsg{ header })
    .and_then([&]() -> unistdpp::Result<void> {
      if (updates.empty()) {
        return {};
      }
      return sock.writeAll(updates.data(),
                           updates.size() * sizeof(UpdateParams));
    })
    .and_then([&]() { return sock.readAll<bool>(); })
    .or_else([&](auto err) {
      std::cerr << "Error sending: " << unistdpp::to_string(err) << "\n";
      sock.close();
    })
    .value_or(false);
}

bool
isInputDevicePath(const char* pathname) {
  // Covers eventN nodes and the by-id/by-path symlink aliases some
  // callers use instead - both resolve to the same struct file.
  constexpr std::string_view prefix = "/dev/input/";
  return strncmp(pathname, prefix.data(), prefix.size()) == 0 &&
         strlen(pathname) < sizeof(OpenInputDevice::path);
}

int
openInputDeviceOrFail(const char* pathname, int flags) {
  auto fd = ClientSocket().openInputDevice(pathname, flags);
  if (!fd) {
    errno = static_cast<int>(fd.error());
    return -1;
  }
  return *fd;
}

bool
sendUpdate(const UpdateParams& params) {
  return ClientSocket().sendUpdate(params);
}

bool
sendUpdateBatch(const std::vector<UpdateParams>& updates) {
  return ClientSocket().sendUpdateBatch(updates);
}
