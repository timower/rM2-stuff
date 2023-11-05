#include "unistdpp/poll.h"

namespace unistdpp {

Result<int>
poll(std::vector<pollfd>& pollfds,
     std::optional<std::chrono::milliseconds> timeout) {

  int result =
    ::poll(pollfds.data(),
           pollfds.size(),
           timeout.has_value() ? static_cast<int>(timeout->count()) : -1);
  if (result == -1) {
    return tl::unexpected(getErrno());
  }

  return result;
}

} // namespace unistdpp
