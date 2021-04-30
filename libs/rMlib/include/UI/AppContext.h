#pragma once

#include <Input.h>
#include <UI/Timer.h>

#include <vector>

namespace rmlib {

class AppContext {
public:
  AppContext(Canvas& fbCanvas) : canvas(fbCanvas) {}

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

  const Canvas& getFbCanvas() const { return canvas; }

private:
  input::InputManager inputManager;
  TimerQueue timers;
  std::vector<Callback> doLaters;
  Canvas& canvas;

  bool mShouldStop = false;
};

} // namespace rmlib
