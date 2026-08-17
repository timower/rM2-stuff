#pragma once

#include <Input.h>
#include <UI/RenderObject.h>
#include <UI/Timer.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace rmlib {

struct FdEntry {
  Callback callback;
  bool enabled = true;
};

// RAII handle for an AppContext::listenFd() registration, mirroring
// TimerHandle: disabling just flags the entry, actual removal happens
// lazily on the next waitForInput() so self-disabling from inside the
// callback that's currently firing stays safe.
struct FdHandle {
  FdHandle() = default;
  explicit FdHandle(std::shared_ptr<FdEntry> entry) : entry(std::move(entry)) {}
  ~FdHandle() { disable(); }

  FdHandle(FdHandle&&) = default;
  FdHandle& operator=(FdHandle&&) = default;
  FdHandle(const FdHandle&) = delete;
  FdHandle& operator=(const FdHandle&) = delete;

  void disable() {
    if (entry != nullptr) {
      entry->enabled = false;
      entry = nullptr;
    }
  }

private:
  std::shared_ptr<FdEntry> entry;
};

class AppContext {
public:
  static ErrorOr<AppContext> makeContext(std::optional<Size> size = {}) {
    auto fb = TRY(fb::FrameBuffer::open(size));
    auto ctx = AppContext(std::move(fb));
    TRY(ctx.inputManager.openAll());
    return ctx;
  }

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
      timers.pop();

      if (top->check()) {
        if (top->repeats()) {
          top->reset();
          timers.emplace(std::move(top));
        }
      } else {
        timers.emplace(std::move(top));
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
  fb::FrameBuffer& getFramebuffer() { return framebuffer; }

  void onDeviceUpdate(Callback fn) {
    onDeviceUpdates.emplace_back(std::move(fn));
  }

  // Caller must keep the returned handle alive for as long as the
  // callback should stay registered - dropping it deregisters the fd.
  [[nodiscard]] FdHandle listenFd(int fd, Callback callback) {
    auto entry =
      std::make_shared<FdEntry>(FdEntry{ std::move(callback), true });
    extraFds.emplace(fd, entry);
    return FdHandle(std::move(entry));
  }

  ErrorOr<std::vector<input::Event>> waitForInput(
    std::optional<std::chrono::microseconds> durantion) {

    const auto milliDuration =
      [durantion]() -> std::optional<std::chrono::milliseconds> {
      if (!durantion.has_value()) {
        return {};
      }
      return std::chrono::duration_cast<std::chrono::milliseconds>(*durantion);
    }();

    std::size_t startDevices = inputManager.numDevices();
    std::vector<input::Event> result;

    // Drop fds whose handle was disabled since the last poll, mirroring how
    // disabled Timers get dropped from the queue in checkTimers().
    for (auto it = extraFds.begin(); it != extraFds.end();) {
      if (it->second->enabled) {
        ++it;
      } else {
        it = extraFds.erase(it);
      }
    }

    if (!extraFds.empty()) {
      std::vector<pollfd> pollFds;
      for (const auto& [fd, _] : extraFds) {
        pollFds.emplace_back(
          pollfd{ .fd = fd, .events = POLLIN, .revents = 0 });
      }

      auto evs = TRY(inputManager.waitForInput(pollFds, milliDuration));

      for (const auto& poll : pollFds) {
        if (!unistdpp::canRead(poll)) {
          continue;
        }
        auto it = extraFds.find(poll.fd);
        if (it != extraFds.end() && it->second->enabled) {
          it->second->callback();
        }
      }

      result = std::move(evs);
    } else {
      auto evs = TRY(inputManager.waitForInput(milliDuration));
      result = std::move(evs);
    }

    if (inputManager.numDevices() != startDevices) {
      for (const auto& onDeviceUpdate : onDeviceUpdates) {
        onDeviceUpdate();
      }
    }

    return result;
  }

  void setRootRenderObject(std::unique_ptr<RenderObject> obj) {
    rootRO = std::move(obj);
  }

  RenderObject& getRootRenderObject() { return *rootRO; }

  void step() {
    rootRO->rebuild(*this, nullptr);
    rootRO->layout(rootConstraints);

    pendingUpdates.clear();
    rootRO->cleanup(framebuffer.canvas, pendingUpdates);
    rootRO->draw(framebuffer.canvas, { 0, 0 }, pendingUpdates);

    if (!pendingUpdates.empty()) {
      framebuffer.doUpdates(pendingUpdates);
    }

    // Run before waitForInput/checkTimers so a doLater() queued from a
    // listenFd callback (which runs inside waitForInput, before this step's
    // own checkTimers) only executes here on the *next* step - i.e. after
    // that step's draw, not before it.
    doAllLaters();

    const auto duration = getNextDuration();
    const auto evsOrError = waitForInput(duration);
    checkTimers();

    if (!evsOrError.has_value()) {
      std::cerr << evsOrError.error().msg << std::endl;
    } else {
      for (const auto& ev : *evsOrError) {
        rootRO->handleInput(ev);
      }
    }

    rootRO->reset();
  }

protected:
  AppContext(fb::FrameBuffer fb) : framebuffer(std::move(fb)) {
    const auto fbSize =
      Size{ framebuffer.canvas.width(), framebuffer.canvas.height() };
    rootConstraints = Constraints{ fbSize, fbSize };
  }

  fb::FrameBuffer framebuffer; // NOLINT (not private)

private:
  input::InputManager inputManager;

  TimerQueue timers;

  // TODO: use handles to destroy these
  std::vector<Callback> doLaters;
  std::vector<Callback> onDeviceUpdates;

  std::unordered_map<int, std::shared_ptr<FdEntry>> extraFds;

  bool mShouldStop = false;

  std::unique_ptr<RenderObject> rootRO;
  Constraints rootConstraints{};

  // Reused across steps to avoid a fresh heap allocation every frame.
  std::vector<UpdateRegion> pendingUpdates;
};

} // namespace rmlib
