#pragma once

#include "unistdpp.h"

#include <sys/ioctl.h>

namespace unistdpp {

template<typename... ExtraArgs>
constexpr auto ioctl =
  FnWrapper<::ioctl, Result<void>(const FD&, int, ExtraArgs...)>{};
}
