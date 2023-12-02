#pragma once

#include "traits.h"

#include <tl/expected.hpp>

#include <iostream>
#include <system_error>

namespace error_details {
template<typename T, typename E>
constexpr auto
getValue(tl::expected<T, E> v) {
  if constexpr (std::is_same_v<T, void>) {
    (void)v; // to suppress unused variable warnings
    return;
  } else {
    return std::move(*v);
  }
}
} // namespace error_details

// NOLINTNEXTLINE
#define TRY(x)                                                                 \
  ({                                                                           \
    auto&& xOrErr = x;                                                         \
    if (!xOrErr.has_value()) {                                                 \
      return tl::unexpected(xOrErr.error());                                   \
    };                                                                         \
    error_details::getValue(std::move(xOrErr));                                \
  })

namespace unistdpp {
template<typename T>
using Result = tl::expected<T, std::errc>;

[[nodiscard]] inline std::errc
getErrno() {
  return static_cast<std::errc>(errno);
}

inline std::string
to_string(std::errc error) { // NOLINT
  return std::make_error_code(error).message();
}

template<typename T, typename E>
T
fatalOnError(tl::expected<T, E> error, std::string_view msg = "") {
  if (!error.has_value()) {
    using namespace std;
    std::cerr << "FATAL: " << msg << to_string(error.error()) << std::endl;
    std::abort();
  }

  if constexpr (std::is_void_v<T>) {
    return;
  } else {
    return *error;
  }
}

template<typename T>
struct WrapperTraits<Result<T>> {
  static_assert(
    std::is_integral_v<T>,
    "Please provide explicit wrapper trait for non integral results");

  static Result<T> ret(T res) {
    if (res == -1) {
      return tl::unexpected(getErrno());
    }
    return res;
  }
};

template<>
struct WrapperTraits<Result<void>> {
  static Result<void> ret(int res) {
    if (res == -1) {
      return tl::unexpected(getErrno());
    }
    return {};
  }
};

} // namespace unistdpp
