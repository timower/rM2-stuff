#include <Device.h>
#include <Input.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <csignal>
#include <fstream>
#include <iostream>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

using namespace rmlib;
using namespace rmlib::input;

namespace {

struct RunningAppInfo {
  pid_t pid;
};

struct AppDescription {
  std::string name;
  std::string description;

  std::string command;
  std::string icon;
};

std::vector<AppDescription> apps;

bool
endsWith(std::string_view a, std::string_view end) {
  if (a.size() < end.size()) {
    return false;
  }

  return a.substr(a.size() - end.size()) == end;
}

std::optional<AppDescription>
readDraftFile(std::string_view path) {
  std::ifstream ifs(path.data());
  if (!ifs.is_open()) {
    return std::nullopt;
  }

  AppDescription result;

  for (std::string line; std::getline(ifs, line);) {
    auto eq = line.find("=");
    if (eq == std::string::npos) {
      continue;
    }

    auto key = line.substr(0, eq);
    auto value = line.substr(eq + 1);
    if (key == "name") {
      result.name = value;
    } else if (key == "desc") {
      result.description = value;
    } else if (key == "call") {
      result.command = value;
    } else if (key == "imgFile") {
      result.icon = value;
    }
  }

  if (result.name.empty()) {
    return std::nullopt;
  }

  return result;
}

/// Reads apps from draft files in `/etc/draft`
void
readApps() {
  constexpr auto path = "/etc/draft";
  const auto paths = device::listDirectory(path);

  for (const auto& path : paths) {
    if (!endsWith(path, ".draft")) {
      std::cerr << "skipping non draft file: " << path << std::endl;
      continue;
    }

    auto app = readDraftFile(path);
    if (!app.has_value()) {
      std::cerr << "error parsing file: " << path << std::endl;
    }
    apps.emplace_back(std::move(*app));
  }
}

struct GestureConfig {
  enum Type { Swipe, Pinch, Tap };
  enum Action { ShowApps, Launch, ShowRunning, NextRunning, PrevRunning };

  Type type;
  std::variant<PinchGesture::Direction, SwipeGesture::Direction> direction;
  int fingers;

  Action action;
  std::string command;

  bool matches(const SwipeGesture& g) const {
    return type == GestureConfig::Swipe &&
           std::get<SwipeGesture::Direction>(direction) == g.direction &&
           fingers == g.fingers;
  }

  bool matches(const PinchGesture& g) const {
    return type == GestureConfig::Pinch &&
           std::get<PinchGesture::Direction>(direction) == g.direction &&
           fingers == g.fingers;
  }

  bool matches(const TapGesture& g) const { return g.fingers == fingers; }
};

struct Config {
  std::vector<GestureConfig> gestures;
};

const static Config default_config = { {
  { GestureConfig::Swipe,
    SwipeGesture::Right,
    2,
    GestureConfig::Launch,
    "LD_PRELOAD=/home/root/librm2fb_client.so.1.0.0 /home/root/tilem" },
  { GestureConfig::Swipe,
    SwipeGesture::Left,
    2,
    GestureConfig::Launch,
    "echo left" },
} };

std::optional<Config>
readConfg() {
  return default_config;
}

pid_t
runCommand(std::string_view cmd) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("Error launching");
    return -1;
  } else if (pid > 0) {
    // Parent process, pid is the child pid
    return pid;
  } else {
    execlp("/bin/sh", "/bin/sh", "-c", cmd.data(), nullptr);
    perror("Error running process");
    return -1;
  }
}

void
doAction(const GestureConfig& config) {
  switch (config.action) {
    case GestureConfig::Launch: {
      std::cout << "Running: " << config.command << std::endl;
      auto pid = runCommand(config.command);
      std::cout << "PID: " << pid << std::endl;
    } break;
  }
}

void
printGesture(const TapGesture& g) {
  std::cout << "Gesture Tap\n";
}

void
printGesture(const PinchGesture& g) {
  std::cout << "Gesture Pinch\n";
}

void
printGesture(const SwipeGesture& g) {
  std::cout << "Gesture Swipe";

  switch (g.direction) {
    case SwipeGesture::Up:
      std::cout << " Up";
      break;
    case SwipeGesture::Down:
      std::cout << " Down";
      break;
    case SwipeGesture::Left:
      std::cout << " Left";
      break;
    case SwipeGesture::Right:
      std::cout << " Right";
      break;
  }

  std::cout << " fingers: " << g.fingers << std::endl;
}

template<typename Gesture>
void
handleGesture(const Config& config, const Gesture& g) {
  printGesture(g);

  for (const auto& gc : config.gestures) {
    if (gc.matches(g)) {
      doAction(gc);
    }
  }
}

void
cleanup(int signal) {
  pid_t childPid;
  while ((childPid = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
    std::cout << "Exited: " << childPid << std::endl;
  }
}

} // namespace

int
main(int argc, char* argv[]) {
  std::signal(SIGCHLD, cleanup);

  InputManager input;
  GestureController gestureController;

  auto deviceType = device::getDeviceType();
  if (!deviceType.has_value()) {
    std::cerr << "Error finding device type\n";
    return -1;
  }

  auto paths = device::getInputPaths(*deviceType);
  if (!input.open(paths.touchPath.data(), paths.touchTransform).has_value()) {
    std::cerr << "Error opening input\n";
    return -1;
  }

  auto config = readConfg();
  if (!config.has_value()) {
    std::cerr << "Error reading config\n";
    return -1;
  }

  readApps();

  std::cout << "Apps:" << std::endl;
  for (const auto& app : apps) {
    std::cout << " - " << app.name << std::endl;
  }

  while (true) {
    auto events = input.waitForInput();

    if (!events.has_value()) {
      std::cerr << "Error reading event\n";
      continue;
    }
    auto gestures = gestureController.handleEvents(*events);

    for (const auto& gesture : gestures) {
      std::visit([&config](const auto& g) { handleGesture(*config, g); },
                 gesture);
    }
  }

  return 0;
}
