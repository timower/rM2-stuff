#include "unistdpp/file.h"

namespace unistdpp {

Result<std::string>
readFile(const std::filesystem::path& path) {
  return open(path.c_str(), O_RDONLY).and_then([](auto fd) {
    std::string buf;
    return lseek(fd, 0, SEEK_END)
      .and_then([&](auto offset) {
        buf.resize(offset);
        return lseek(fd, 0, SEEK_SET);
      })
      .and_then([&](auto) { return fd.readAll(buf.data(), buf.size()); })
      .map([&buf]() { return std::move(buf); });
  });
}

} // namespace unistdpp
