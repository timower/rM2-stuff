#include "unistdpp/pipe.h"

namespace unistdpp {

Result<Pipe>
pipe() {
  int fds[2];
  int res = ::pipe(fds);
  if (res == -1) {
    return tl::unexpected(getErrno());
  }

  return Pipe{ FD{ fds[0] }, FD{ fds[1] } };
}

} // namespace unistdpp
