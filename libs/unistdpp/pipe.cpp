#include "unistdpp/pipe.h"

#include <fcntl.h>

namespace unistdpp {

Result<Pipe>
pipe(bool closeExec) {
  std::array<int, 2> fds{};
  int res = ::pipe2(fds.data(), closeExec ? O_CLOEXEC : 0);
  if (res == -1) {
    return tl::unexpected(getErrno());
  }

  return Pipe{ FD{ fds[0] }, FD{ fds[1] } };
}

} // namespace unistdpp
