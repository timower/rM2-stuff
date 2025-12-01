#pragma once

#include <unistdpp/socket.h>

constexpr std::string_view control_sock_addr = "/var/run/rm2fb.control.sock";

struct ControlInterface {
  struct Client {
    pid_t pid;
    bool active;

    char name[32];
  };

  virtual unistdpp::Result<std::vector<Client>> getClients() = 0;
  virtual unistdpp::Result<int> getFramebuffer(pid_t pid) = 0;

  virtual unistdpp::Result<void> switchTo(pid_t pid) = 0;
  virtual unistdpp::Result<void> setLauncher(pid_t pid) = 0;
};

struct ControlClient : ControlInterface {
  unistdpp::FD sock;

  unistdpp::Result<void> init();

  unistdpp::Result<std::vector<Client>> getClients() override;
  unistdpp::Result<int> getFramebuffer(pid_t pid) override;

  unistdpp::Result<void> switchTo(pid_t pid) override;
  unistdpp::Result<void> setLauncher(pid_t pid) override;
};

struct ControlServer {
  ControlInterface& iface;
  unistdpp::FD sock;

  ControlServer(ControlInterface& iface) : iface(iface) {}

  unistdpp::Result<void> maybeInit();
  unistdpp::Result<void> handleMsg();
};
