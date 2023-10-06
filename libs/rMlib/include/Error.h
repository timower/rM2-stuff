#pragma once

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

  static Error fromErrno() { return Error{ strerror(errno) }; }
  static tl::unexpected<Error> errn() { return tl::unexpected(fromErrno()); }
};

template<typename T, typename E = Error>
using ErrorOr = tl::expected<T, E>;

template<typename E = Error>
using OptError = tl::expected<void, E>;

#define TRY(x)                                                                 \
  ({                                                                           \
    auto xOrErr = x;                                                           \
    if (!xOrErr.has_value()) {                                                 \
      return tl::unexpected(xOrErr.error());                                   \
    };                                                                         \
    std::move(*xOrErr);                                                        \
  })
