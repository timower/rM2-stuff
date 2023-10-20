#include "unistdpp/file.h"

namespace unistdpp {

Result<std::string>
readFile(const std::filesystem::path& path) {
  auto fd = TRY(open(path.c_str(), O_RDONLY));

  std::string buf;
  auto offset = TRY(lseek(fd, 0, SEEK_END));
  buf.resize(offset);
  TRY(lseek(fd, 0, SEEK_SET));

  auto readSize = TRY(fd.readAll(buf.data(), buf.size()));
  buf.resize(readSize);
  return buf;
}

} // namespace unistdpp
