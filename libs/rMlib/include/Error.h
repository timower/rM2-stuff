#pragma once

#include <cassert>
#include <cstring>

#include <string>
#include <variant>

struct NoError {};

struct Error {
  std::string msg;

  static Error errn() { return Error{ strerror(errno) }; }
};

template<typename T, typename Err = Error>
class ErrorOr;

template<typename Err = Error>
using OptError = ErrorOr<NoError, Err>;

// TODO: allow references in ErrorOr -> if T is reference, store reference
// wrapper.
template<typename T, typename Err>
class ErrorOr {
  constexpr static bool isOptError = std::is_same_v<T, NoError>;
  static_assert(!std::is_same_v<T, Err>);

public:
  template<typename U,
           typename = std::enable_if_t<std::is_constructible_v<T, U>>>
  ErrorOr(U&& val) : value(std::forward<U>(val)) {}

  template<typename U = T,
           typename = std::enable_if_t<std::is_same_v<U, NoError>>>
  ErrorOr() : value(NoError{}) {}

  ErrorOr(Err e) : value(std::move(e)) {}

  bool isError() const { return std::holds_alternative<Err>(value); }

  const Err& getError() const {
    assert(isError());
    return std::get<Err>(value);
  }

  Err& getError() {
    assert(isError());
    return std::get<Err>(value);
  }

  template<typename U = T,
           typename = std::enable_if_t<!std::is_same_v<U, NoError>>>
  T& operator*() {
    assert(!isError());
    return std::get<T>(value);
  }

  template<typename U = T,
           typename = std::enable_if_t<!std::is_same_v<U, NoError>>>
  const T& operator*() const {
    assert(!isError());
    return std::get<T>(value);
  }

  template<typename U = T,
           typename = std::enable_if_t<!std::is_same_v<U, NoError>>>
  auto* operator->() {
    assert(!isError());
    return &std::get<T>(value);
  }

  template<typename U = T,
           typename = std::enable_if_t<!std::is_same_v<U, NoError>>>
  const auto* operator->() const {
    assert(!isError());
    return &std::get<T>(value);
  }

private:
  std::variant<T, Err> value;
};

#define TRY(x)                                                                 \
  ({                                                                           \
    auto xOrErr = x;                                                           \
    if (xOrErr.isError()) {                                                    \
      return xOrErr.getError();                                                \
    };                                                                         \
    std::move(*xOrErr);                                                        \
  })
