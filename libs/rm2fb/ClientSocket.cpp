// Update-sending code shared by every rm2fb client variant (Client.cpp,
// ClientSwtcon.cpp) - kept in its own translation unit so
// rm2fb_client_swtcon can pull it in without depending on the rest of
// Client.cpp (see ClientSwtcon.cpp's top comment).

#include "Client.h"

#include "rm2fb/Message.h"
#include "rm2fb/SharedBuffer.h"

#include "unistdpp/error.h"

unistdpp::FD&
getControlSocket() {
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

bool
sendUpdate(const UpdateParams& params) {
  auto& clientSock = getControlSocket();
  if (!clientSock.isValid()) {
    return false;
  }

  return sendMessage(clientSock, UnixClientMsg{ params })
    .and_then([&]() { return clientSock.readAll<bool>(); })
    .or_else([&](auto err) {
      std::cerr << "Error sending: " << unistdpp::to_string(err) << "\n";
      clientSock.close();
    })
    .value_or(false);
}

bool
sendUpdateBatch(const std::vector<UpdateParams>& updates) {
  auto& clientSock = getControlSocket();
  if (!clientSock.isValid()) {
    return false;
  }

  const auto header =
    UpdateBatchHeader{ .count = static_cast<int32_t>(updates.size()) };

  return sendMessage(clientSock, UnixClientMsg{ header })
    .and_then([&]() -> unistdpp::Result<void> {
      if (updates.empty()) {
        return {};
      }
      return clientSock.writeAll(updates.data(),
                                 updates.size() * sizeof(UpdateParams));
    })
    .and_then([&]() { return clientSock.readAll<bool>(); })
    .or_else([&](auto err) {
      std::cerr << "Error sending: " << unistdpp::to_string(err) << "\n";
      clientSock.close();
    })
    .value_or(false);
}
