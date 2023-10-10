#include "unistdpp/unistdpp.h"

namespace unistdpp {

Result<void>
FD::readAll(void* buf, std::size_t size) const {
  std::size_t read = 0;
  while (read < size) {
    auto res = ::read(fd, reinterpret_cast<char*>(buf) + read, size - read);
    if (res == -1) {
      return tl::unexpected(getErrno());
    }
    read += res;
  }

  return {};
}

Result<void>
FD::writeAll(const void* buf, std::size_t size) const {
  std::size_t written = 0;
  while (written < size) {
    auto res =
      ::write(fd, reinterpret_cast<const char*>(buf) + written, size - written);
    if (res == -1) {
      return tl::unexpected(getErrno());
    }
    written += res;
  }

  return {};
}
} // namespace unistdpp
