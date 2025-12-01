#pragma once

#include "unistdpp.h"

#include <system_error>
#include <tl/expected.hpp>

namespace unistdpp {

struct Pipe {
  FD readPipe;
  FD writePipe;
};

Result<Pipe>
pipe(bool closeExec = true);

} // namespace unistdpp
