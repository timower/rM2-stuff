#include "App.h"

#include <unistdpp/pipe.h>

#include <Device.h>

#include <algorithm>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <sys/wait.h>
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

  setpgid(0, 0);

  std::cout << "Running: " << cmd << std::endl;
  execlp("/bin/sh", "/bin/sh", "-c", cmd.data(), nullptr);
  perror("Error running process");
  return -1;
}

void
stop(std::shared_ptr<AppRunInfo> runInfo) {
  const auto pid = runInfo->pid;

  if (runInfo->paused) {
    kill(-pid, SIGCONT);
  }

  int res = kill(-pid, SIGTERM);
  if (res != 0) {
    perror("Error killing!");
  }
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
    result.iconPath = std::string(iconDir) + '/' + result.icon + ".png";
  }

  return std::optional(std::move(result));
}

std::optional<Canvas>
AppDescription::getIcon() const {
  struct CacheValue {
    ImageCanvas image;
    std::filesystem::file_time_type time;
  };
  static std::unordered_map<std::string, CacheValue> imageCache;

  std::error_code ec;
  auto time = std::filesystem::last_write_time(iconPath, ec);
  if (ec) {
    return std::nullopt;
  }

  auto it = imageCache.find(iconPath);
  if (it != imageCache.end()) {
    if (time == it->second.time) {
      return it->second.image.canvas;
    }

    // Icon has been modified, remove from cache.
    imageCache.erase(it);
  }

  std::cout << "Parsing image from: " << iconPath << std::endl;
  auto iconImage = ImageCanvas::load(iconPath.c_str());
  if (!iconImage.has_value()) {
    return std::nullopt;
  }

  auto canvas = iconImage->canvas;
  std::cout << "With components: " << canvas.components() << std::endl;
  imageCache.emplace_hint(
    it, iconPath, CacheValue{ .image = std::move(*iconImage), .time = time });
  return canvas;
}

std::vector<AppDescription>
readAppFiles(std::string_view directory) {
  const auto iconPath = std::string(directory) + "/icons";
  const auto paths = device::listDirectory(directory);

  std::vector<AppDescription> result;

  for (const auto& path : paths) {
    auto appDesc = AppDescription::read(path, iconPath);
    if (!appDesc.has_value()) {
      std::cerr << "error parsing file: " << path << std::endl;
      continue;
    }

    result.emplace_back(std::move(*appDesc));
  }

  return result;
}

void
App::updateDescription(AppDescription desc) {
  mDescription = std::move(desc);
  iconCanvas = mDescription.getIcon();
}

bool
App::launch() {
  if (isRunning()) {
    assert(false && "Shouldn't be called if the app is already running");
    return false;
  }

  auto pid = runCommand(description().command);
  if (pid == -1) {
    return false;
  }

  auto runInfo = std::make_shared<AppRunInfo>();
  runInfo->pid = pid;

  this->runInfo = runInfo;

  AppManager::getInstance().runInfos.emplace_back(std::move(runInfo));

  return true;
}

void
App::stop() {
  assert(isRunning());

  ::stop(runInfo.lock());
}

void
App::pause(std::optional<MemoryCanvas> screen) {
  assert(isRunning() && !isPaused());

  auto lockedInfo = runInfo.lock();

  int res = kill(-lockedInfo->pid, SIGSTOP);
  if (res != 0) {
    perror("Failed to send SIGSTOP");
    return;
  }

  lockedInfo->paused = true;
  savedFb = std::move(screen);
}

void
App::resume(rmlib::fb::FrameBuffer* fb) {
  assert(isPaused());

  if (savedFb.has_value() && fb != nullptr) {
    fb->canvas.copy({ 0, 0 }, savedFb->canvas, fb->canvas.rect());
    fb->doUpdate(
      fb->canvas.rect(), fb::Waveform::GC16Fast, fb::UpdateFlags::FullRefresh);
    savedFb.reset();
  }

  auto lockedInfo = runInfo.lock();
  kill(-lockedInfo->pid, SIGCONT);
  lockedInfo->paused = false;
}

AppManager&
AppManager::getInstance() {
  static AppManager instance;
  return instance;
}

bool
AppManager::update() {
  bool anyKilled = false;

  while (auto res = pipe.readPipe.readAll<pid_t>()) {
    auto pid = *res;
    anyKilled = true;

    runInfos.erase(
      std::remove_if(runInfos.begin(),
                     runInfos.end(),
                     [pid](auto& info) { return info->pid == pid; }),
      runInfos.end());
  }

  return anyKilled;
}

void
AppManager::onSigChild(int sig) {
  auto& inst = AppManager::getInstance();

  pid_t childPid = 0;
  while ((childPid = waitpid(static_cast<pid_t>(-1), nullptr, WNOHANG)) > 0) {

    std::cout << "Killed: " << childPid << "\n";

    auto v = inst.pipe.writePipe.writeAll(childPid);

    if (!v.has_value()) {
      std::cerr << "Error in writing pid: " << to_string(v.error()) << "\n";
    }
  }
}

AppManager::AppManager() : pipe(unistdpp::fatalOnError(unistdpp::pipe())) {
  unistdpp::setNonBlocking(pipe.readPipe);
  std::signal(SIGCHLD, onSigChild);
}

AppManager::~AppManager() {
  for (auto runInfo : runInfos) {
    ::stop(runInfo);
  }
}
