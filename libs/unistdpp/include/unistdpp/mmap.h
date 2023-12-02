#pragma once

#include "unistdpp.h"

#include <memory>

namespace unistdpp {
struct MmapDeleter {
  std::size_t size;
  void operator()(void* ptr) const;
};

using MmapPtr = std::unique_ptr<void, MmapDeleter>;

Result<MmapPtr>
mmap(void* addr, size_t len, int prot, int flags, const FD& fd, off_t off);
} // namespace unistdpp
