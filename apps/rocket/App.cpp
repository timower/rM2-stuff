#include "App.h"

#include <Device.h>

#include <csignal>
#include <fstream>
#include <iostream>

#include <unistd.h>

using namespace rmlib;

namespace {

pid_t
runCommand(std::string_view cmd) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("Error launching");
    return -1;
  }

  if (pid > 0) {
    // Parent process, pid is the child pid
    return pid;
  }

  std::cout << "Running: " << cmd << std::endl;
  setpgid(0, 0);
  execlp("/bin/sh", "/bin/sh", "-c", cmd.data(), nullptr);
  perror("Error running process");
  return -1;
}

bool
endsWith(std::string_view a, std::string_view end) {
  if (a.size() < end.size()) {
    return false;
  }

  return a.substr(a.size() - end.size()) == end;
}
} // namespace

std::optional<AppDescription>
AppDescription::read(std::string_view path, std::string_view iconDir) {
  std::ifstream ifs(path.data());
  if (!ifs.is_open()) {
    return std::nullopt;
  }

  AppDescription result;
  result.path = path;

  for (std::string line; std::getline(ifs, line);) {
    auto eq = line.find('=');
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

  if (result.name.empty() || result.command.empty()) {
    return std::nullopt;
  }

  if (!result.icon.empty()) {
    auto iconPath = std::string(iconDir) + '/' + result.icon + ".png";
    std::cout << "Parsing image from: " << iconPath << std::endl;
    result.iconImage = ImageCanvas::load(iconPath.c_str());
    if (result.iconImage.has_value()) {
      std::cout << result.iconImage->canvas.components() << std::endl;
    }
  }

  return std::optional(std::move(result));
}

std::vector<AppDescription>
readAppFiles(std::string_view directory) {
  const auto iconPath = std::string(directory) + "/icons";
  const auto paths = device::listDirectory(directory);

  std::vector<AppDescription> result;

  for (const auto& path : paths) {
    if (!endsWith(path, ".draft")) {
      std::cerr << "skipping non draft file: " << path << std::endl;
      continue;
    }

    auto appDesc = AppDescription::read(path, iconPath);
    if (!appDesc.has_value()) {
      std::cerr << "error parsing file: " << path << std::endl;
      continue;
    }

    result.emplace_back(std::move(*appDesc));
  }

  return result;
}

bool
App::launch() {
  if (runInfo.has_value()) {
    assert(false && "Shouldn't be called if the app is already running");
    return false;
  }

  auto pid = runCommand(description.command);
  if (pid == -1) {
    return false;
  }

  runInfo = AppRunInfo{ pid };

  return true;
}

void
App::stop() {
  assert(isRunning());

  if (isPaused()) {
    kill(-runInfo->pid, SIGCONT);
  }

  kill(-runInfo->pid, SIGTERM);
}

void
App::pause(std::optional<MemoryCanvas> screen) {
  assert(isRunning() && !isPaused());

  kill(-runInfo->pid, SIGSTOP);
  runInfo->paused = true;
  savedFb = std::move(screen);
}

void
App::resume(rmlib::fb::FrameBuffer* fb) {
  assert(isPaused());

  if (savedFb.has_value() && fb != nullptr) {
    copy(fb->canvas, { 0, 0 }, savedFb->canvas, fb->canvas.rect());
    fb->doUpdate(
      fb->canvas.rect(), fb::Waveform::GC16Fast, fb::UpdateFlags::FullRefresh);
    savedFb.reset();
  }

  kill(-runInfo->pid, SIGCONT);
  runInfo->paused = false;
}
