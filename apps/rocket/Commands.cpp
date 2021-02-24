#include "Commands.h"

#include <iostream>
#include <sstream>

// TODO: This is almost becoming a lisp interpreter, maybe move to one at some
// point?
namespace {

ErrorOr<std::function<void()>>
getCommandFn(Launcher& launcher, std::string_view command);

std::vector<std::string_view>
split(std::string_view str, std::string_view delims = " ") {
  std::vector<std::string_view> output;

  for (auto first = str.data(), second = str.data(), last = first + str.size();
       second != last && first != last;
       first = second + 1) {
    second =
      std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

    if (first != second) {
      output.emplace_back(first, second - first);
    }
  }

  return output;
}

using Tokens = std::vector<std::string_view>;

ErrorOr<std::vector<Tokens>>
tokenize(std::string_view str) {
  std::vector<Tokens> output;
  output.reserve(1);

  std::vector<std::string_view> tokens;

  bool inQuotes = false;
  for (auto first = str.data(), second = str.data(), last = first + str.size();
       first < last;
       ++second) {
    if (second == last && first != second) {
      tokens.emplace_back(first, second - first);
      break;
    }

    if (*second == ' ' && !inQuotes && first != second) {
      tokens.emplace_back(first, second - first);
      first = second + 1;
    } else if (*second == ';' && !inQuotes) {
      if (first != second) {
        tokens.emplace_back(first, second - first);
      }
      output.emplace_back(std::move(tokens));
      tokens.clear();
      first = second + 1;
    } else if (*second == '"') {
      if (inQuotes) {
        inQuotes = false;
        tokens.emplace_back(first, second - first);
      } else {
        inQuotes = true;
      }
      first = second + 1;
    }
  }

  if (!tokens.empty()) {
    output.emplace_back(std::move(tokens));
  }

  if (inQuotes) {
    return Error{ "Unclosed quotes" };
  }

  std::cout << "parsed: ";
  for (const auto& tokens : output) {
    for (const auto& token : tokens) {
      std::cout << '"' << token << "\" ";
    }
    std::cout << "; ";
  }
  std::cout << std::endl;

  return output;
}

template<typename V>
ErrorOr<V>
parseArg(std::string_view arg) {
  static_assert(!std::is_same_v<V, V>, "No parser for type");
}

template<>
ErrorOr<std::string_view>
parseArg<std::string_view>(std::string_view arg) {
  return arg;
}

template<>
ErrorOr<std::string>
parseArg<std::string>(std::string_view arg) {
  return std::string(arg);
}

template<>
ErrorOr<ActionConfig>
parseArg<ActionConfig>(std::string_view arg) {
  using namespace rmlib::input;

  auto tokens = split(arg, ":");
  if (tokens.empty()) {
    return Error{ "Empty action" };
  }

  ActionConfig result;
  if (tokens.front() == "Swipe") {
    result.type = ActionConfig::Swipe;
    if (tokens.size() != 3) {
      return Error{ "Expected Swipe:direction:fingers" };
    }

    if (tokens[1] == "Up") {
      result.direction = SwipeGesture::Up;
    } else if (tokens[1] == "Down") {
      result.direction = SwipeGesture::Down;
    } else if (tokens[1] == "Left") {
      result.direction = SwipeGesture::Left;
    } else if (tokens[1] == "Right") {
      result.direction = SwipeGesture::Right;
    } else {
      return Error{ "Unknown direction: " + std::string(tokens[1]) };
    }

    auto num = std::string(tokens[2]);
    result.fingers = atoi(num.c_str());
  } else if (tokens.front() == "Pinch") {
    result.type = ActionConfig::Pinch;
    if (tokens.size() != 3) {
      return Error{ "Expected Pinch:direction:fingers" };
    }

    if (tokens[1] == "In") {
      result.direction = PinchGesture::In;
    } else if (tokens[1] == "Out") {
      result.direction = PinchGesture::Out;
    } else {
      return Error{ "Unknown direction: " + std::string(tokens[1]) };
    }

    auto num = std::string(tokens[2]);
    result.fingers = atoi(num.c_str());
  } else if (tokens.front() == "Tap") {
    result.type = ActionConfig::Tap;

    if (tokens.size() != 2) {
      return Error{ "Expected Tap:fingers" };
    }

    auto num = std::string(tokens[1]);
    result.fingers = atoi(num.c_str());
  } else {
    return Error{ "Unknown gesture: " + std::string(tokens.front()) };
  }

  return result;
}

template<typename... Args>
ErrorOr<std::tuple<Args...>>
anyError(ErrorOr<Args>... args) {
  bool hasError = (args.isError() || ... || false);
  if (!hasError) {
    return std::tuple{ *args... };
  }

  return Error{ ((args.isError() ? args.getError().msg + ", " : "") + ... +
                 "") };
}

template<typename T>
struct ToOwningImpl {
  using type = T;
};

template<>
struct ToOwningImpl<std::string_view> {
  using type = std::string;
};

// Converts non owning types to owning types. Ex string_view to string.
// Used when curry-ing the commands for actions.
template<typename T>
using ToOwning = typename ToOwningImpl<T>::type;

struct Command {

  template<typename... Args>
  static ErrorOr<std::tuple<Args...>> parseArgs(
    const std::vector<std::string_view>& args) {
    if (args.size() != sizeof...(Args) + 1) {
      return Error{ "Invalid number of arguments for '" + std::string(args[0]) +
                    "', expected " + std::to_string(sizeof...(Args)) + " got " +
                    std::to_string(args.size() - 1) };
    }

    // Skip command name.
    auto argIt = std::next(args.begin());
    (void)argIt;

    // Parse each argument.
    auto argOrErrorTuple =
      std::tuple<ErrorOr<Args>...>{ parseArg<Args>(*argIt++)... };

    // Convert from tuple of errors to error of tuple.
    return std::apply([](auto&... args) { return anyError(args...); },
                      argOrErrorTuple);
  }

  template<typename... Args>
  constexpr Command(CommandResult (*fn)(Launcher&, Args...),
                    std::string_view help) noexcept
    : fn(reinterpret_cast<void*>(fn)), help(help) {
    cmdFn = [](auto* fn,
               Launcher& launcher,
               const std::vector<std::string_view>& args) -> CommandResult {
      auto parsedArgs = TRY(parseArgs<Args...>(args));

      auto argTuple = std::tuple_cat(std::tuple<Launcher&>{ launcher },
                                     std::move(parsedArgs));
      auto* fnPtr = reinterpret_cast<CommandResult (*)(Launcher&, Args...)>(fn);
      return std::apply(fnPtr, argTuple);
    };

    parseFn = [](auto* fn,
                 Launcher& launcher,
                 const std::vector<std::string_view>& args)
      -> ErrorOr<std::function<CommandResult()>> {
      auto parsedArgs = TRY(parseArgs<ToOwning<Args>...>(args));

      auto argTuple = std::tuple_cat(std::tuple<Launcher&>{ launcher },
                                     std::move(parsedArgs));
      return [fn, args = std::move(argTuple)]() {
        auto* fnPtr =
          reinterpret_cast<CommandResult (*)(Launcher&, Args...)>(fn);
        return std::apply(fnPtr, args);
      };
    };
  }

  CommandResult operator()(Launcher& launcher,
                           const std::vector<std::string_view>& args) const {
    return cmdFn(fn, launcher, args);
  }

  ErrorOr<std::function<CommandResult()>> parse(
    Launcher& launcher,
    const std::vector<std::string_view>& args) const {
    return parseFn(fn, launcher, args);
  }

  void* fn;
  std::string_view help;

  CommandResult (*cmdFn)(void*,
                         Launcher&,
                         const std::vector<std::string_view>&);

  ErrorOr<std::function<CommandResult()>> (
    *parseFn)(void*, Launcher&, const std::vector<std::string_view>&);
};

CommandResult
help(Launcher&);

CommandResult
launch(Launcher& launcher, std::string_view name) {
  auto* app = launcher.getApp(name);
  if (app == nullptr) {
    return Error{ "App not found " + std::string(name) };
  }

  launcher.switchApp(*app);

  return std::string("Launching: ") + std::string(name);
}

CommandResult
show(Launcher& launcher) {
  launcher.drawAppsLauncher();
  return "OK";
}

CommandResult
hide(Launcher& launcher) {
  launcher.closeLauncher();
  return "OK";
}

template<typename It>
It
getNext(It start, It begin, It end) {
  auto it = start;
  it++;
  while (it != start) {
    if (it == end) {
      it = begin;
    }

    if (it->isRunning()) {
      break;
    }

    it++;
  }

  return it;
}

enum class SwitchTarget { Next, Prev, Last };

template<>
ErrorOr<SwitchTarget>
parseArg<SwitchTarget>(std::string_view arg) {
  if (arg == "next" || arg == "Next") {
    return SwitchTarget::Next;
  }
  if (arg == "prev" || arg == "Prev") {
    return SwitchTarget::Prev;
  }
  if (arg == "last" || arg == "Last") {
    return SwitchTarget::Last;
  }
  return Error{ "Expected on of <next|prev|last>" };
}

CommandResult
switchTo(Launcher& launcher, SwitchTarget target) {

  switch (target) {
    case SwitchTarget::Next: {
      auto start = std::find_if(
        launcher.apps.begin(), launcher.apps.end(), [&launcher](auto& app) {
          return app.description.path == launcher.currentAppPath;
        });
      if (launcher.currentAppPath.empty() || start == launcher.apps.end()) {
        return "No apps running";
      }

      auto it = getNext(start, launcher.apps.begin(), launcher.apps.end());
      launcher.switchApp(*it);
    } break;

    case SwitchTarget::Prev: {
      auto start = std::find_if(
        launcher.apps.rbegin(), launcher.apps.rend(), [&launcher](auto& app) {
          return app.description.path == launcher.currentAppPath;
        });
      if (launcher.currentAppPath.empty() || start == launcher.apps.rend()) {
        return "No apps running";
      }

      auto it = getNext(start, launcher.apps.rbegin(), launcher.apps.rend());
      launcher.switchApp(*it);
    } break;

    case SwitchTarget::Last: {
      auto* currentApp = launcher.getCurrentApp();
      App* lastApp = nullptr;
      for (auto& app : launcher.apps) {
        if (app.runInfo.has_value() && &app != currentApp &&
            (lastApp == nullptr ||
             app.lastActivated > lastApp->lastActivated)) {
          lastApp = &app;
        }
      }

      if (lastApp == nullptr) {
        return "No other apps";
      }
      launcher.switchApp(*lastApp);
    } break;
  }

  return "OK";
}

CommandResult
onAction(Launcher& launcher, ActionConfig action, std::string_view command) {
  auto fnOrError = getCommandFn(launcher, command);
  if (fnOrError.isError()) {
    return Error{ "Can't add action: " + fnOrError.getError().msg +
                  " for command: \"" + std::string(command) + "\"" };
  }

  launcher.config.actions.emplace_back(Action{ action, *fnOrError });
  return "OK";
}

CommandResult
kill(Launcher& launcher, std::string_view name) {
  auto* app = launcher.getApp(name);
  if (app == nullptr) {
    return Error{ "Unknown app: " + std::string(name) };
  }
  app->stop();
  return "OK";
}

enum class InputType { Touch, Pen, Keys };

template<>
ErrorOr<InputType>
parseArg<InputType>(std::string_view arg) {
  if (arg == "touch") {
    return InputType::Touch;
  }
  if (arg == "pen") {
    return InputType::Pen;
  }
  if (arg == "keys") {
    return InputType::Keys;
  }
  return Error{ "Expected <touch|pen|keys>" };
}

CommandResult
grab(Launcher& launcher, InputType input) {
  switch (input) {
    case InputType::Touch:
      launcher.inputFds->touch.grab();
      break;
    case InputType::Pen:
      launcher.inputFds->pen.grab();
      break;
    case InputType::Keys:
      launcher.inputFds->key.grab();
      break;
  }
  return "OK";
}

CommandResult
ungrab(Launcher& launcher, InputType input) {
  switch (input) {
    case InputType::Touch:
      launcher.inputFds->touch.ungrab();
      break;
    case InputType::Pen:
      launcher.inputFds->pen.ungrab();
      break;
    case InputType::Keys:
      launcher.inputFds->key.ungrab();
      break;
  }
  return "OK";
}

// clang-format off
const std::unordered_map<std::string_view, Command> commands = {
  { "help",   { help,   "- Show help" } },
  { "launch", { launch, "- launch <app name> - Start or switch to app" } },
  { "kill",   { kill,   "- kill <app name> - Stop an app"}},
  { "show",   { show,   "- Show the launcher" } },
  { "hide",   { hide,   "- Hide the launcher" } },
  { "grab",   { grab,   "- grab <input> - Grabs the given input" } },
  { "ungrab", { ungrab, "- ungrab <input> - Releases the given input" } },
  { "switch", { switchTo,
  "- switch <next|prev|last> - Switch to the next, previous or last running app"
  } },
  { "on",     { onAction,
  "- on <gesture> <command> - Execute command when the given action occurs"
  } },
};
// clang-format on

CommandResult
help(Launcher&) {
  std::stringstream ss;

  ss << "Commands:\n";

  for (const auto& [name, cmd] : commands) {
    ss << "\t" << name << " " << cmd.help << "\n";
  }

  return ss.str();
}

ErrorOr<std::function<void()>>
getCommandFn(Launcher& launcher, std::string_view command) {
  auto cmds = TRY(tokenize(command));

  std::vector<std::function<CommandResult()>> results;
  for (const auto& tokens : cmds) {
    if (tokens.empty()) {
      continue;
    }

    auto cmdIt = commands.find(tokens.front());
    if (cmdIt == commands.end()) {
      return Error{ std::string("Command ") + std::string(tokens.front()) +
                    " not found" };
    }

    auto parsedFn = TRY(cmdIt->second.parse(launcher, tokens));
    results.emplace_back(std::move(parsedFn));
  }

  return [fns = std::move(results)]() {
    for (const auto& fn : fns) {
      auto res = fn();
      if (res.isError()) {
        std::cerr << res.getError().msg << std::endl;
        break;
      }
    }
  };
}

} // namespace

CommandResult
doCommand(Launcher& launcher, std::string_view command) {
  auto cmds = TRY(tokenize(command));

  std::string result;
  for (const auto& tokens : cmds) {
    if (tokens.empty()) {
      continue;
    }

    auto cmdIt = commands.find(tokens.front());
    if (cmdIt == commands.end()) {
      return Error{ std::string("Command ") + std::string(tokens.front()) +
                    " not found" };
    }

    result += TRY(cmdIt->second(launcher, tokens));
  }

  return result;
}
