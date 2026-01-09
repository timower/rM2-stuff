#pragma once

#include "unistdpp.h"

#include <sys/mman.h>

namespace unistdpp {

constexpr auto shm_open =
  unistdpp::FnWrapper<::shm_open, Result<FD>(const char*, int, mode_t)>{};

}
