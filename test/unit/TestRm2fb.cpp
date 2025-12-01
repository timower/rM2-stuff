#include <catch2/catch_test_macros.hpp>
#include <future>
#include <string>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include "rm2fb/ControlSocket.h"

class MockControlInterface : public ControlInterface {
public:
  pid_t switched_to_pid = 0;
  pid_t launcher_pid = 0;
  pid_t fb_pid = 0;

  unistdpp::Result<std::vector<Client>> getClients() override {
    return std::vector<Client>{
      {
        .pid = 1,
        .active = true,
        .name = "test1",
      },
      {
        .pid = 2,
        .active = false,
        .name = "test2",
      },
    };
  }

  unistdpp::Result<int> getFramebuffer(pid_t pid) override {
    fb_pid = pid;
    int fds[2];
    if (pipe(fds) != 0) {
      return tl::unexpected(std::errc::io_error);
    }
    close(fds[1]); // close write end
    return fds[0]; // return read end
  }

  unistdpp::Result<void> switchTo(pid_t pid) override {
    switched_to_pid = pid;
    return {};
  }

  unistdpp::Result<void> setLauncher(pid_t pid) override {
    launcher_pid = pid;
    return {};
  }
};

TEST_CASE("ControlSocket", "[rm2fb]") {
  std::string socket_path = "/tmp/rm2fb-test.sock";
  unlink(socket_path.c_str()); // Make sure the socket does not exist

  MockControlInterface mock_iface;
  ControlServer server(mock_iface);
  REQUIRE(server.maybeInit(socket_path.c_str()).has_value());

  ControlClient client;
  REQUIRE(client.init(socket_path.c_str()).has_value());

  SECTION("getClients") {
    auto server_future = std::async(std::launch::async, [&] {
      return server.handleMsg();
    });

    auto clients = client.getClients();
    server_future.get(); // wait for server to finish

    REQUIRE(clients.has_value());
    REQUIRE(clients->size() == 2);
    REQUIRE(clients->at(0).pid == 1);
    REQUIRE(clients->at(0).active == true);
    REQUIRE(std::string(clients->at(0).name) == "test1");
    REQUIRE(clients->at(1).pid == 2);
    REQUIRE(clients->at(1).active == false);
    REQUIRE(std::string(clients->at(1).name) == "test2");
  }

  SECTION("switchTo") {
    auto server_future = std::async(std::launch::async, [&] {
      return server.handleMsg();
    });

    auto result = client.switchTo(123);
    server_future.get();

    REQUIRE(result.has_value());
    REQUIRE(mock_iface.switched_to_pid == 123);
  }

  SECTION("setLauncher") {
    auto server_future = std::async(std::launch::async, [&] {
      return server.handleMsg();
    });

    auto result = client.setLauncher(456);
    server_future.get();

    REQUIRE(result.has_value());
    REQUIRE(mock_iface.launcher_pid == 456);
  }

  SECTION("getFramebuffer") {
    auto server_future = std::async(std::launch::async, [&] {
      return server.handleMsg();
    });

    auto fd = client.getFramebuffer(789);
    server_future.get();

    REQUIRE(fd.has_value());
    REQUIRE(mock_iface.fb_pid == 789);
    REQUIRE(*fd >= 0);
    REQUIRE(fcntl(*fd, F_GETFD) != -1);
    close(*fd);
  }
}

TEST_CASE("ControlServer::maybeInit") {
  std::string socket_path = "/tmp/rm2fb-test-maybeinit.sock";
  unlink(socket_path.c_str());

  MockControlInterface mock_iface;
  ControlServer server(mock_iface);
  REQUIRE(!server.sock.isValid());

  SECTION("Initializes socket") {
    auto result = server.maybeInit(socket_path.c_str());
    REQUIRE(result.has_value());
    REQUIRE(server.sock.isValid());

    // Check that the socket is bound
    struct sockaddr_un addr;
    socklen_t len = sizeof(addr);
    REQUIRE(getsockname(server.sock.fd, (struct sockaddr*)&addr, &len) == 0);
    REQUIRE(std::string(addr.sun_path) == socket_path);
  }

  SECTION("Does nothing if socket is already valid") {
    server.sock = unistdpp::socket(AF_UNIX, SOCK_DGRAM, 0).value();
    int old_fd = server.sock.fd;

    auto result = server.maybeInit(socket_path.c_str());
    REQUIRE(result.has_value());
    REQUIRE(server.sock.fd == old_fd);
  }
}
