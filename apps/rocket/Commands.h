#pragma once

#include "Launcher.h"

#include <Error.h>

#include <string>
#include <string_view>
#include <variant>

using CommandResult = ErrorOr<std::string>;

/// Execute the command.
/// \return nullopt if an error occured, the result otherwise.
CommandResult
doCommand(Launcher& launcher, std::string_view command);
