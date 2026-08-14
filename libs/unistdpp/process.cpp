#include "unistdpp/process.h"

#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

namespace unistdpp {

Result<FD>
pidfdOpen(pid_t pid) {
  auto fd = FD(static_cast<int>(syscall(SYS_pidfd_open, pid, 0)));
  if (!fd.isValid()) {
    return tl::unexpected(getErrno());
  }
  return fd;
}

} // namespace unistdpp
