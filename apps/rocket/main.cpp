#include "CommandSocket.h"
#include "Commands.h"
#include "Launcher.h"

#include <csignal>
#include <fstream>
#include <iostream>

#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr auto config_path_suffix = ".config/rocket/config";

Launcher launcher;

void
cleanup(int signal) {
  pid_t childPid = 0;
  while ((childPid = waitpid(static_cast<pid_t>(-1), nullptr, WNOHANG)) > 0) {
    std::cout << "Exited: " << childPid << std::endl;
    launcher.handleChildStop(childPid);
  }
}

void
stop(int signal) {
  launcher.shouldStop = true;
}

bool
parseConfig() {
  const auto* home = getenv("HOME");
  if (home == nullptr) {
    return false;
  }

  auto configPath = std::string(home) + '/' + config_path_suffix;

  std::ifstream ifs(configPath);
  if (!ifs.is_open()) {
    // TODO: default config
    std::cerr << "Error opening config path\n";
    return false;
  }

  for (std::string line; std::getline(ifs, line);) {
    auto result = doCommand(launcher, line);
    if (result.isError()) {
      std::cerr << result.getError().msg << std::endl;
    } else {
      std::cout << *result << std::endl;
    }
  }

  return true;
}

int
runCommand(int argc, char* argv[]) {
  std::string command;
  for (int i = 1; i < argc; i++) {
    command.append(argv[i]);
    if (i != argc - 1) {
      command.push_back(' ');
    }
  }

  if (command.empty()) {
    std::cerr << "Rocket running, usage: " << argv[0] << " <command>\n";
    return EXIT_FAILURE;
  }

  if (!CommandSocket::send(command)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

} // namespace

int
main(int argc, char* argv[]) {
  if (CommandSocket::alreadyRunning()) {
    return runCommand(argc, argv);
  }

  if (argc != 1) {
    std::cerr << "Rocket socket not running\n";
    return EXIT_FAILURE;
  }

  std::signal(SIGCHLD, cleanup);
  std::signal(SIGINT, stop);
  std::signal(SIGTERM, stop);

  if (auto err = launcher.init(); err.isError()) {
    std::cerr << "Error init: " << err.getError().msg << std::endl;
    return EXIT_FAILURE;
  }

  if (!parseConfig()) {
    return EXIT_FAILURE;
  }

  launcher.run();

  launcher.closeLauncher();

  return EXIT_SUCCESS;
}
