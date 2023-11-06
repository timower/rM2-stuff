#pragma once

#include "unistdpp.h"

#include <fcntl.h>
#include <filesystem>

namespace unistdpp {

constexpr auto open = FnWrapper<::open, Result<FD>(const char*, int)>{};
constexpr auto lseek =
  FnWrapper<::lseek, Result<off_t>(const FD&, off_t, int)>{};

constexpr auto ftruncate =
  FnWrapper<::ftruncate, Result<void>(const FD&, off_t)>{};

template<typename... ExtraArgs>
constexpr auto fcntl =
  FnWrapper<::fcntl, Result<int>(const FD&, int, ExtraArgs...)>{};

Result<std::string>
readFile(const std::filesystem::path& path);

Result<std::string>
readlink(const std::filesystem::path& path);

} // namespace unistdpp
