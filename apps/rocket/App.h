#pragma once

#include <Canvas.h>
#include <FrameBuffer.h>

#include <unistdpp/pipe.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

struct AppRunInfo {
  pid_t pid = -1;
  bool paused = false;
};

struct AppDescription {
  std::string path; // path of app draft file, is unique.

  std::string name;
  std::string description;

  std::string command;
  std::string icon;

  std::string iconPath;
  std::optional<rmlib::ImageCanvas> getIcon() const;

  static std::optional<AppDescription> read(std::string_view path,
                                            std::string_view iconDir);
};

std::vector<AppDescription>
readAppFiles(std::string_view directory);

class App {
public:
  App(AppDescription desc)
    : mDescription(std::move(desc)), iconImage(mDescription.getIcon()) {}

  void updateDescription(AppDescription desc);

  bool isRunning() const { return !runInfo.expired(); }
  bool isPaused() const { return isRunning() && runInfo.lock()->paused; }

  /// Starts a new instance of the app if it's not already running.
  /// \returns True if a new instance was started.
  bool launch();

  void stop();

  void pause(std::optional<rmlib::MemoryCanvas> screen = std::nullopt);
  void resume(rmlib::fb::FrameBuffer* fb = nullptr);

  const AppDescription& description() const { return mDescription; }

  const std::optional<rmlib::ImageCanvas>& icon() const { return iconImage; }
  const std::optional<rmlib::MemoryCanvas>& savedFB() const { return savedFb; }
  void resetSavedFB() { savedFb.reset(); }

  void setRemoveOnExit() { shouldRemove = true; }
  bool shouldRemoveOnExit() const { return shouldRemove; }

private:
  AppDescription mDescription;

  std::weak_ptr<AppRunInfo> runInfo;

  std::optional<rmlib::ImageCanvas> iconImage;
  std::optional<rmlib::MemoryCanvas> savedFb;

  // Indicates that the app should be removed when it exists
  bool shouldRemove = false;
};

class AppManager {
public:
  static AppManager& getInstance();

  bool update();
  const unistdpp::FD& getWaitFD() const { return pipe.readPipe; }

private:
  friend class App;
  unistdpp::Pipe pipe;

  std::vector<std::shared_ptr<AppRunInfo>> runInfos;

  static void onSigChild(int sig);

  AppManager();
  ~AppManager();
};
