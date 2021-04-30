#pragma once

#include <chrono>
#include <queue>

#include <UI/Util.h>

namespace rmlib {
class Timer;

struct TimerHandle {
  TimerHandle(std::shared_ptr<Timer> timer) : timer(std::move(timer)) {}
  ~TimerHandle();

  TimerHandle() = default;
  TimerHandle(TimerHandle&&) = default;
  TimerHandle& operator=(TimerHandle&&) = default;

  TimerHandle(const TimerHandle&) = delete;
  TimerHandle& operator=(const TimerHandle&) = delete;

  void disable();

private:
  std::shared_ptr<Timer> timer;
};

class Timer {
  using clock = std::chrono::steady_clock;

public:
  static std::pair<std::shared_ptr<Timer>, TimerHandle> makeTimer(
    std::chrono::microseconds duration,
    Callback callback,
    std::optional<std::chrono::microseconds> repeat = std::nullopt) {

    auto timer =
      std::shared_ptr<Timer>(new Timer(duration, std::move(callback), repeat));
    auto handle = TimerHandle(timer);

    return { std::move(timer), std::move(handle) };
  }

  bool check() const {
    if (!enabled) {
      return true;
    }

    if (clock::now() >= triggerTime) {
      callback();
      return true;
    }

    return false;
  }

  std::chrono::microseconds getDuration() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(triggerTime -
                                                                 clock::now());
  }

  bool repeats() const { return repeat.has_value(); }

  void reset() {
    assert(repeats());
    triggerTime = clock::now() + *repeat;
  }

  void disable() {
    enabled = false;
    repeat = std::nullopt;
  }

private:
  Timer(std::chrono::microseconds duration,
        Callback callback,
        std::optional<std::chrono::microseconds> repeat = std::nullopt)
    : repeat(repeat)
    , triggerTime(clock::now() + duration)
    , callback(std::move(callback)) {}

  friend struct TimerCmp;

  std::optional<std::chrono::microseconds> repeat;
  clock::time_point triggerTime;
  Callback callback;
  bool enabled = true;
};

struct TimerCmp {
  bool operator()(const std::shared_ptr<Timer>& lhs,
                  const std::shared_ptr<Timer>& rhs) {
    return lhs->triggerTime > rhs->triggerTime;
  }
};

inline void
TimerHandle::disable() {
  if (timer != nullptr) {
    timer->disable();
    timer = nullptr;
  }
}

inline TimerHandle::~TimerHandle() {
  disable();
}

using TimerQueue = std::priority_queue<std::shared_ptr<Timer>,
                                       std::vector<std::shared_ptr<Timer>>,
                                       TimerCmp>;

} // namespace rmlib
