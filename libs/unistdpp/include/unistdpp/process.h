#pragma once

#include "unistdpp.h"

#include <sys/types.h>

namespace unistdpp {

/// Pollable fd for `pid`, ready for read once it exits.
/// Linux 5.3+; matches the reMarkable 2 kernel (5.4.70).
Result<FD>
pidfdOpen(pid_t pid);

} // namespace unistdpp
