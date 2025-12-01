#include "App.h"

#include <unistdpp/pipe.h>

#include <Device.h>

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <sys/wait.h>
#include <unistd.h>

using std::optional;

using namespace rmlib;

namespace {

unistdpp::Result<pid_t>
runCommand(std::string_view cmd) {
  auto pipes = TRY(unistdpp::pipe());

  pid_t pid = fork();

  if (pid == -1) {
    return tl::unexpected(unistdpp::getErrno());
  }

  if (pid > 0) {
    pipes.writePipe.close();
    // Parent process, read child pid.
    auto res = pipes.readPipe.readAll<pid_t>();
    waitpid(pid, nullptr, 0);
    return res;
  }

  pipes.readPipe.close();

  setsid();

  pid_t pid2 = fork();
  if (pid2 == -1) {
    perror("Error launching");
    exit(EXIT_FAILURE);
  }

  if (pid2 > 0) {
    // intermediate parent, exit.
    unistdpp::fatalOnError(pipes.writePipe.writeAll(pid2));
    exit(EXIT_SUCCESS);
  }

  pipes.writePipe.close();

  std::cerr << "Running: " << cmd << std::endl;
  execlp("/bin/sh", "/bin/sh", "-c", cmd.data(), nullptr);

  perror("Error launching");
  exit(EXIT_FAILURE);
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

  return std::move(result);
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
  auto pidOrErr = runCommand(description().command);
  if (!pidOrErr) {
    std::cerr << "Error launching: " << unistdpp::to_string(pidOrErr.error())
              << "\n";
    return false;
  }

  pid = *pidOrErr;
  return true;
}
