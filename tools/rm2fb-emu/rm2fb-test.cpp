#include "Socket.h"

#include <Error.h>

#include <toml++/toml.h>
#include <unistdpp/socket.h>

#include <filesystem>
#include <iostream>
#include <variant>
#include <vector>

using namespace unistdpp;

namespace {

struct ScreenshotAction {
  std::string name;
};

struct WaitAction {
  int time;
};

using Action = std::variant<ScreenshotAction, WaitAction>;

struct Test {
  std::string command;
  int timeout;

  std::vector<Action> actions;
};

template<typename T>
ErrorOr<T>
getValue(const toml::table& table, std::string_view key) {
  auto val = table[key].value<T>();
  if (!val) {
    return Error::make("Missing key: " + std::string(key));
  }
  return *val;
}

ErrorOr<Action>
parseAction(const toml::table& table) {
  auto doName = TRY(getValue<std::string>(table, "do"));

  if (doName == "wait") {
  } else if (doName == "screenshot") {
  }

  return Error::make("No such action: " + doName);
}

ErrorOr<Test>
parseTest(std::filesystem::path tomlFile) {
  try {
    auto table = toml::parse_file(tomlFile.c_str());

    auto command = TRY(getValue<std::string>(table, "command"));
    auto timeout = table["timeout"].value_or(0);

    const auto* array = table["actions"].as_array();
    if (array == nullptr) {
      return Error::make("No actions array");
    }

    std::vector<Action> actions;
    for (const auto& elem : *array) {
      const auto* action = elem.as_table();
      if (action == nullptr) {
        return Error::make("Action should be a table");
      }
      actions.push_back(TRY(parseAction(*action)));
    }

    return Test{
      .command = command,
      .timeout = timeout,
      .actions = std::move(actions),
    };

  } catch (const toml::parse_error& err) {
    return Error::make(std::string(err.description()));
  }
}

} // namespace

int
main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cout << "Usage: " << argv[0] << " <ip> <port> <test file>\n";
  }

  auto test = fatalOnError(parseTest(argv[3]));

  int port = atoi(argv[2]);
  auto sock = getClientSock(argv[1], port);
  if (!sock.has_value()) {
    std::cout << "Couldn't get tcp socket: " << toString(sock.error()) << "\n";
    return EXIT_FAILURE;
  }

  bool running = true;
  while (running) {
  }
}
