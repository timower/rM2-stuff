#include "unistdpp/pipe.h"

namespace unistdpp {

Result<Pipe>
pipe() {
  std::array<int, 2> fds{};
  int res = ::pipe(fds.data());
  if (res == -1) {
    return tl::unexpected(getErrno());
  }

  return Pipe{ FD{ fds[0] }, FD{ fds[1] } };
}

} // namespace unistdpp
