#pragma once

#include <unistdpp/error.h>

#include <cassert>
#include <cstring>

#include <string>
#include <variant>

#include <tl/expected.hpp>

struct Error {
  std::string msg;

  static tl::unexpected<Error> make(std::string msg) {
    return tl::unexpected(Error{ std::move(msg) });
  }

  Error(std::errc err) : msg(std::make_error_code(err).message()) {}
  Error(std::string msg) : msg(std::move(msg)) {}
};

template<typename T, typename E = Error>
using ErrorOr = tl::expected<T, E>;

template<typename E = Error>
using OptError = tl::expected<void, E>;
