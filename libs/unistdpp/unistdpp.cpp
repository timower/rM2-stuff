#include "unistdpp/unistdpp.h"

#include "unistdpp/file.h"

namespace unistdpp {

Result<int>
FD::readAll(void* buf, std::size_t size) const {
  std::size_t read = 0;
  while (read < size) {
    // NOLINTNEXTLINE
    auto res = ::read(fd, reinterpret_cast<char*>(buf) + read, size - read);
    if (res == -1) {
      return tl::unexpected(getErrno());
    }

    if (res == 0) {
      break;
    }

    read += res;
  }

  return read;
}

Result<void>
FD::writeAll(const void* buf, std::size_t size) const {
  std::size_t written = 0;
  while (written < size) {
    auto res = // NOLINTNEXTLINE
      ::write(fd, reinterpret_cast<const char*>(buf) + written, size - written);
    if (res == -1) {
      return tl::unexpected(getErrno());
    }

    written += res;
  }

  return {};
}

Result<void>
setNonBlocking(const FD& fd) {
  auto flags = TRY(fcntl<>(fd, F_GETFL));
  TRY(fcntl<int>(fd, F_SETFL, flags | O_NONBLOCK));
  return {};
}

} // namespace unistdpp
