#pragma once

#include <unistdpp/error.h>

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <iostream>
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

inline std::string
to_string(const Error& err) { // NOLINT
  return err.msg;
}

template<typename T, typename E = Error>
using ErrorOr = tl::expected<T, E>;

template<typename E = Error>
using OptError = tl::expected<void, E>;

template<typename T, typename E = Error>
T
fatalOnError(tl::expected<T, E> error) {
  if (!error.has_value()) {
    using namespace std;
    std::cerr << "FATAL: " << to_string(error.error()) << std::endl;
    std::abort();
  }

  if constexpr (std::is_void_v<T>) {
    return;
  } else {
    return *error;
  }
}
