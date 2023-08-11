#pragma once

#include <Input.h>
#include <UI/Timer.h>

#include <vector>

namespace rmlib {

class AppContext {
public:
  AppContext(fb::FrameBuffer& framebuffer) : framebuffer(framebuffer) {}

  TimerHandle addTimer(
    std::chrono::microseconds duration,
    Callback trigger,
    std::optional<std::chrono::microseconds> repeat = std::nullopt) {
    auto [timer, handle] =
      Timer::makeTimer(duration, std::move(trigger), repeat);
    timers.emplace(std::move(timer));
    return std::move(handle);
  }

  std::optional<std::chrono::microseconds> getNextDuration() const {
    if (timers.empty()) {
      return std::nullopt;
    }

    auto duration = timers.top()->getDuration();
    if (duration < std::chrono::microseconds(0)) {
      return std::chrono::microseconds(0);
    }
    return duration;
  }

  void checkTimers() {
    while (!timers.empty()) {
      std::shared_ptr<Timer> top = timers.top();
      if (top->check()) {
        timers.pop();

        if (top->repeats()) {
          top->reset();
          timers.emplace(std::move(top));
        }
      } else {
        break;
      }
    }
  }

  void stop() { mShouldStop = true; }

  bool shouldStop() const { return mShouldStop; }

  void doLater(Callback fn) { doLaters.emplace_back(std::move(fn)); }
  void doAllLaters() {
    for (const auto& doLater : doLaters) {
      doLater();
    }
    doLaters.clear();
  }

  input::InputManager& getInputManager() { return inputManager; }

  const Canvas& getFbCanvas() const { return framebuffer.canvas; }
  const fb::FrameBuffer& getFramebuffer() const { return framebuffer; }

  void onDeviceUpdate(Callback fn) {
    onDeviceUpdates.emplace_back(std::move(fn));
  }

  void listenFd(int fd, Callback callback) {
    extraFds.emplace(fd, std::move(callback));
  }

  ErrorOr<std::vector<input::Event>> waitForInput(
    std::optional<std::chrono::microseconds> durantion) {

    std::size_t startDevices = inputManager.devices.size();
    std::vector<input::Event> result;

    if (!extraFds.empty()) {
      int maxFd = std::max_element(extraFds.begin(),
                                   extraFds.end(),
                                   [](const auto& a, const auto& b) {
                                     return a.first < b.first;
                                   })
                    ->first;

      fd_set set;
      FD_ZERO(&set);
      for (const auto& [fd, _] : extraFds) {
        FD_SET(fd, &set);
      }

      auto evs = TRY(inputManager.waitForInput(set, maxFd, durantion));

      for (const auto& [fd, cb] : extraFds) {
        if (FD_ISSET(fd, &set)) {
          cb();
        }
      }

      result = std::move(evs);
    } else {
      auto evs = TRY(inputManager.waitForInput(durantion));
      result = std::move(evs);
    }

    if (inputManager.devices.size() != startDevices) {
      for (const auto& onDeviceUpdate : onDeviceUpdates) {
        onDeviceUpdate();
      }
    }

    return result;
  }

private:
  fb::FrameBuffer& framebuffer;
  input::InputManager inputManager;

  TimerQueue timers;
  // TODO: use handles to destroy these
  std::vector<Callback> doLaters;
  std::vector<Callback> onDeviceUpdates;

  std::unordered_map<int, Callback> extraFds;

  bool mShouldStop = false;
};

} // namespace rmlib
