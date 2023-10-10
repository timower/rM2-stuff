#pragma once

#include "unistdpp.h"

#include <fcntl.h>
#include <filesystem>

namespace unistdpp {

constexpr auto open = FnWrapper<::open, Result<FD>(const char*, int)>{};
constexpr auto lseek =
  FnWrapper<::lseek, Result<off_t>(const FD&, off_t, int)>{};

Result<std::string>
readFile(const std::filesystem::path& path);

} // namespace unistdpp
