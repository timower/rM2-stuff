#pragma once

#include <Canvas.h>
#include <FrameBuffer.h>

#include <unistdpp/pipe.h>

#include <optional>
#include <string>
#include <vector>

struct AppDescription {
  std::string path; // path of app draft file, is unique.

  std::string name;
  std::string description;

  std::string command;
  std::string icon;

  std::string iconPath;
  std::optional<rmlib::Canvas> getIcon() const;

  static std::optional<AppDescription> read(std::string_view path,
                                            std::string_view iconDir);
};

std::vector<AppDescription>
readAppFiles(std::string_view directory);

class App {
public:
  App(AppDescription desc)
    : mDescription(std::move(desc)), iconCanvas(mDescription.getIcon()) {}

  void updateDescription(AppDescription desc);

  /// Starts a new instance of the app if it's not already running.
  /// \returns True if a new instance was started.
  bool launch();
  pid_t getLaunchPid() const { return pid; }

  const AppDescription& description() const { return mDescription; }
  const std::optional<rmlib::Canvas>& icon() const { return iconCanvas; }

private:
  AppDescription mDescription;
  pid_t pid = 0;

  std::optional<rmlib::Canvas> iconCanvas;
};
