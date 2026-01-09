#include "unistdpp/mmap.h"

#include <sys/mman.h>

namespace unistdpp {

void
MmapDeleter::operator()(void* ptr) const {
  munmap(ptr, size);
}

Result<MmapPtr>
mmap(void* addr, size_t len, int prot, int flags, const FD& fd, off_t off) {
  void* res = ::mmap(addr, len, prot, flags, fd.fd, off);
  if (res == MAP_FAILED) {
    return tl::unexpected(getErrno());
  }

  return MmapPtr(res, MmapDeleter{ len });
}
} // namespace unistdpp
