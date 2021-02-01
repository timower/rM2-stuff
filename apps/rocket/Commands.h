#pragma once

#include "Launcher.h"

#include <string>
#include <string_view>
#include <variant>

struct Error {
  std::string msg;
};

template<typename T>
using ErrorOr = std::variant<T, Error>;

template<typename T>
constexpr bool
isError(const ErrorOr<T>& e) {
  return std::holds_alternative<Error>(e);
}

template<typename T>
constexpr const Error&
getError(const ErrorOr<T>& e) {
  return std::get<Error>(e);
}

template<typename T>
T&
operator*(ErrorOr<T>& e) {
  assert(!isError(e));
  return std::get<T>(e);
}

using CommandResult = ErrorOr<std::string>;

/// Execute the command.
/// \return nullopt if an error occured, the result otherwise.
CommandResult
doCommand(Launcher& launcher, std::string_view command);
