#include <catch2/catch_test_macros.hpp>

// unistdpp
#include <unistdpp/file.h>
#include <unistdpp/pipe.h>
#include <unistdpp/poll.h>
#include <unistdpp/socket.h>
#include <unistdpp/unistdpp.h>

#include <iostream>

using namespace unistdpp;

const std::filesystem::path assets_path = ASSETS_PATH;

namespace Catch {
template<typename T>
struct StringMaker<Result<T>> {
  static std::string convert(const Result<T>& value) {
    return value.has_value() ? StringMaker<T>::convert(*value)
                             : "Error: " + toString(value.error());
  }
};
template<>
struct StringMaker<Result<void>> {
  static std::string convert(const Result<void>& value) {
    return value.has_value() ? "OK" : "Error: " + toString(value.error());
  }
};
} // namespace Catch

TEST_CASE("error", "[unistdpp]") {
  REQUIRE(toString(std::errc::invalid_argument) == "Invalid argument");
}

TEST_CASE("open", "[unistdpp]") {
  FD fd;
  SECTION("FD move") {
    auto res = unistdpp::open((assets_path / "test.txt").c_str(), O_RDONLY);
    REQUIRE(res.has_value());
    fd = std::move(*res);

    REQUIRE(fd.isValid());
    REQUIRE(fd);
    REQUIRE_FALSE(res->isValid());
    REQUIRE_FALSE(*res);
  }

  FD fd2 = std::move(fd);
  REQUIRE(fd2.isValid());
  REQUIRE_FALSE(fd.isValid());
  fd2.close();
  REQUIRE_FALSE(fd2.isValid());
}

TEST_CASE("lseek", "[unistdpp]") {
  auto fd = FD{};

  auto res = unistdpp::lseek(fd, 0, SEEK_SET);
  REQUIRE_FALSE(res.has_value());
  REQUIRE(res.error() == std::errc::bad_file_descriptor);
}

TEST_CASE("readFile", "[unistdpp]") {
  SECTION("non existing") {
    auto err = readFile(assets_path / "doesn't_exist.txt");
    REQUIRE_FALSE(err.has_value());
    REQUIRE(err.error() == std::errc::no_such_file_or_directory);
  }

  SECTION("basic reading") {
    auto result = unistdpp::readFile(assets_path / "test.txt");
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "test");
  }

  SECTION("empty file") {
    auto emptyRes = unistdpp::readFile(assets_path / "empty.txt");
    REQUIRE(emptyRes.has_value());
    REQUIRE(emptyRes->empty());
  }

  SECTION("sysfs") {
    auto res = readFile("/sys/devices/system/cpu/present");
    REQUIRE(res);
    REQUIRE((*res)[0] == '0');
    REQUIRE(res->size() < 100);
  }
}

TEST_CASE("pipe", "[unistdpp]") {
  auto pipe = unistdpp::pipe();
  REQUIRE(pipe.has_value());

  auto [readFd, writeFd] = std::move(*pipe);

  REQUIRE_FALSE(readFd.writeAll(12));

  struct Test {
    int a;
    float b;
  };

  REQUIRE(writeFd.writeAll(Test{ 55, 1.2f }).has_value());

  auto res = readFd.readAll<Test>();
  REQUIRE(res.has_value());
  REQUIRE(res->a == 55);
  REQUIRE(res->b == 1.2f);
}

TEST_CASE("poll", "[unistdpp]") {
  auto pipe1 = unistdpp::pipe();
  auto pipe2 = unistdpp::pipe();
  REQUIRE(pipe1.has_value());
  REQUIRE(pipe2.has_value());

  auto [read1, write1] = std::move(*pipe1);
  auto [read2, write2] = std::move(*pipe2);

  std::vector<pollfd> pollfds;
  pollfds.emplace_back(waitFor(read1, Wait::READ));
  pollfds.emplace_back(waitFor(read2, Wait::READ));

  constexpr auto nowait = std::chrono::milliseconds(0);

  auto res = unistdpp::poll(pollfds, nowait);
  REQUIRE(res.has_value());
  REQUIRE(*res == 0);

  pollfds.emplace_back(waitFor(write1, Wait::WRITE));
  res = unistdpp::poll(pollfds, nowait);
  REQUIRE(res);
  REQUIRE(*res == 1);
  REQUIRE(canWrite(pollfds.back()));
  pollfds.pop_back();

  const char test[] = "test";
  REQUIRE(write1.writeAll(test, sizeof(test)).has_value());

  res = unistdpp::poll(pollfds, nowait);
  REQUIRE(res.has_value());
  REQUIRE(*res == 1);

  REQUIRE(canRead(pollfds[0]));
  REQUIRE_FALSE(canRead(pollfds[1]));

  res = unistdpp::poll(pollfds, std::nullopt);
  REQUIRE(res.has_value());
  REQUIRE(*res == 1);

  REQUIRE(write2.writeAll(test, sizeof(test)));

  res = unistdpp::poll(pollfds, std::nullopt);
  REQUIRE(res.has_value());
  REQUIRE(*res == 2);

  REQUIRE(canRead(pollfds[0]));
  REQUIRE(canRead(pollfds[1]));

  char buf[512];
  REQUIRE(read1.readAll(buf, sizeof(test)).has_value());
  res = unistdpp::poll(pollfds, std::nullopt);
  REQUIRE(res.has_value());
  REQUIRE(*res == 1);

  REQUIRE(read2.readAll(buf, sizeof(test)).has_value());
  res = unistdpp::poll(pollfds, nowait);
  REQUIRE(res.has_value());
  REQUIRE(*res == 0);

  REQUIRE_FALSE(canRead(pollfds[0]));
  REQUIRE_FALSE(canRead(pollfds[1]));
}

TEST_CASE("TCP Socket", "[unistdpp]") {
  auto serverSock = unistdpp::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(serverSock.has_value());
  auto clientSock = unistdpp::socket(AF_INET, SOCK_STREAM, 0);
  REQUIRE(clientSock.has_value());

  REQUIRE_FALSE(clientSock->readAll<char>().has_value());

  int yes = 1;
  REQUIRE(unistdpp::setsockopt(
    *serverSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)));

  auto failAddr = Address::fromHostPort("foo.blah", 4444);
  REQUIRE_FALSE(failAddr.has_value());
  REQUIRE(failAddr.error() == std::errc::invalid_argument);

  constexpr auto port = 43224;
  auto serverAddr = Address::fromHostPort(INADDR_ANY, port);
  REQUIRE(unistdpp::bind(*serverSock, serverAddr));
  REQUIRE(unistdpp::listen(*serverSock, 1));

  auto clientConnectAddr = Address::fromHostPort("127.0.0.1", port);
  REQUIRE(clientConnectAddr.has_value());
  REQUIRE(unistdpp::connect(*clientSock, *clientConnectAddr));

  // TODO: allow retreiving client address easily.
  auto acceptFd = unistdpp::accept(*serverSock, nullptr, nullptr);
  REQUIRE(acceptFd.has_value());

  const char testMsg[] = "Testing";
  REQUIRE(acceptFd->writeAll(testMsg, sizeof(testMsg)));
  REQUIRE(clientSock->writeAll(testMsg, sizeof(testMsg)));

  char buf[512];
  REQUIRE(clientSock->readAll(buf, sizeof(testMsg)));
  REQUIRE(memcmp(buf, testMsg, sizeof(testMsg)) == 0);

  memset(buf, 0, 512);
  REQUIRE(acceptFd->readAll(buf, sizeof(testMsg)));
  REQUIRE(memcmp(buf, testMsg, sizeof(testMsg)) == 0);
}

TEST_CASE("Unix Socket", "[unistdpp]") {
  auto serverSock = unistdpp::socket(AF_UNIX, SOCK_DGRAM, 0);
  REQUIRE(serverSock.has_value());

  constexpr auto test_socket = "/tmp/unistdpp-test.sock";
  unlink(test_socket);

  REQUIRE(unistdpp::bind(*serverSock, Address::fromUnixPath(test_socket)));

  auto clientSock = unistdpp::socket(AF_UNIX, SOCK_DGRAM, 0);
  REQUIRE(clientSock.has_value());

  // auto-bind only works on linux.
#if __linux__
  REQUIRE(unistdpp::bind(*clientSock, Address::fromUnixPath(nullptr)));
#else
  unlink("/tmp/client.sock");
  REQUIRE(
    unistdpp::bind(*clientSock, Address::fromUnixPath("/tmp/client.sock")));
#endif

  REQUIRE(unistdpp::connect(*clientSock, Address::fromUnixPath(test_socket)));

  const char testMsg[] = "Test";
  REQUIRE(unistdpp::sendto(*clientSock, testMsg, sizeof(testMsg), 0, nullptr));

  char buf[512];

  Address recvAddr;
  auto size = unistdpp::recvfrom(
    *serverSock, reinterpret_cast<void*>(buf), sizeof(buf), 0, &recvAddr);
  REQUIRE(size);
  REQUIRE(*size == sizeof(testMsg));
  REQUIRE(memcmp(buf, testMsg, sizeof(testMsg)) == 0);

  REQUIRE(recvAddr.type() == AF_UNIX);

  REQUIRE(
    unistdpp::sendto(*serverSock, testMsg, sizeof(testMsg), 0, &recvAddr));

  auto size2 = unistdpp::recvfrom(
    *clientSock, reinterpret_cast<void*>(buf), sizeof(buf), 0, nullptr);
  REQUIRE(size2);
  REQUIRE(*size2 == sizeof(testMsg));
  REQUIRE(memcmp(buf, testMsg, sizeof(testMsg)) == 0);
}

Result<void>
tryTest(bool fail) {
  auto sock = TRY(unistdpp::socket(AF_INET, SOCK_DGRAM, 0));
  if (fail) {
    auto fd = TRY(
      unistdpp::open((assets_path / "doesn't_exist.txt").c_str(), O_RDONLY));
  }
  TRY(unistdpp::bind(sock, Address::fromHostPort(INADDR_ANY, 0)));
  return {};
}

TEST_CASE("Try macro", "[unistdpp]") {
  REQUIRE(tryTest(false));
  auto err = tryTest(true);
  REQUIRE_FALSE(err);
  REQUIRE(err.error() == std::errc::no_such_file_or_directory);
}
