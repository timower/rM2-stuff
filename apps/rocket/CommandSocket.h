#pragma once

#include <functional>
#include <string>
#include <string_view>

struct CommandSocket {
  CommandSocket(std::function<std::string(std::string_view)> callback)
    : callback(std::move(callback)) {}
  ~CommandSocket();

  static bool alreadyRunning();
  static bool send(std::string_view cmd);

  /// Opens the socket, returns false if it fails.
  bool init();

  /// Returns the fd that should be selected on.
  int getFd();

  /// Accepts an incoming connection or reads from the existing connect.
  /// Should be called after selecting on
  void process();

  std::function<std::string(std::string_view)> callback;

  int serverSock = -1;
  int client = -1;
};
