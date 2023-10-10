#pragma once

#include "unistdpp.h"

#include <chrono>
#include <optional>
#include <vector>

#include <poll.h>

namespace unistdpp {

enum class Wait {
  READ = POLLIN,
  WRITE = POLLOUT,
  READ_WRITE = POLLIN | POLLOUT,
};

inline pollfd
waitFor(const FD& fd, Wait wait) {
  return pollfd{ .fd = fd.fd, .events = static_cast<short>(wait) };
}

inline bool
canRead(const pollfd& fd) {
  return (fd.revents & POLLIN) != 0;
}

inline bool
canWrite(const pollfd& fd) {
  return (fd.revents & POLLOUT) != 0;
}

Result<int>
poll(std::vector<pollfd>& pollfds,
     std::optional<std::chrono::milliseconds> timeout = std::nullopt);

} // namespace unistdpp
