#include "Input.h"

#include <iostream>
#include <thread>

#include <sys/select.h>

#include <SDL.h>

namespace rmlib::input {

void
InputDeviceBase::grab() {}

void
InputDeviceBase::ungrab() {}

InputDeviceBase::~InputDeviceBase() {}

namespace {
struct FakeInputDevice : public InputDeviceBase {
  FakeInputDevice() : InputDeviceBase(0, nullptr, "test") {}

  void flood() final {}
  OptError<> readEvents(std::vector<Event>& out) final { return NoError{}; }
};
} // namespace

InputManager::InputManager() {}

InputManager::~InputManager() {}

ErrorOr<BaseDevices>
InputManager::openAll(bool monitor) {
  auto fakeDev = std::make_unique<FakeInputDevice>();
  auto& fakeDevRef = *fakeDev;
  devices.emplace("Test", std::move(fakeDev));

  return BaseDevices{ fakeDevRef, fakeDevRef, fakeDevRef };
}

ErrorOr<std::vector<Event>>
InputManager::waitForInput(fd_set& fdSet,
                           int maxFd,
                           std::optional<std::chrono::microseconds> timeout) {
  static bool down = false;

  std::thread selectThread([maxFd, &fdSet, &timeout]() {
    if (maxFd == 0 && !FD_ISSET(0, &fdSet)) {
      return;
    }

    auto tv = timeval{ 0, 0 };
    if (timeout.has_value()) {
      constexpr auto second_in_usec =
        std::chrono::microseconds(std::chrono::seconds(1)).count();
      tv.tv_sec =
        std::chrono::duration_cast<std::chrono::seconds>(*timeout).count();
      tv.tv_usec = timeout->count() - (tv.tv_sec * second_in_usec);
    }

    auto ret = select(maxFd + 1,
                      &fdSet,
                      nullptr,
                      nullptr,
                      !timeout.has_value() ? nullptr : &tv);

    if (ret < 0) {
      perror("Select on input failed");
      return;
    }

    if (ret == 0) {
      return;
    }

    SDL_Event event;
    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);
  });

  SDL_Event event;
  if (timeout.has_value()) {
    SDL_WaitEventTimeout(&event, timeout->count() / 1000);
  } else {
    SDL_WaitEvent(&event);
  }

  selectThread.join();

  TouchEvent ev;
  ev.id = 1;
  ev.pressure = 1;
  ev.slot = 1;

  std::vector<Event> result;
  switch (event.type) {
    case SDL_QUIT:
      std::exit(0);
      return Error{ "stop" };
      break;
    case SDL_USEREVENT:

      break;
    case SDL_MOUSEMOTION:
      if (down) {
        auto x = event.motion.x * EMULATE_SCALE;
        auto y = event.motion.y * EMULATE_SCALE;
        ev.type = TouchEvent::Move;
        ev.location = { x, y };
        result.push_back(ev);
      }
      break;
    case SDL_MOUSEBUTTONDOWN:
      if (event.button.button == SDL_BUTTON_LEFT) {
        auto x = event.motion.x * EMULATE_SCALE;
        auto y = event.motion.y * EMULATE_SCALE;
        down = true;
        ev.type = TouchEvent::Down;
        ev.location = { x, y };
        result.push_back(ev);
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        auto x = event.motion.x * EMULATE_SCALE;
        auto y = event.motion.y * EMULATE_SCALE;
        down = false;
        ev.type = TouchEvent::Up;
        ev.location = { x, y };
        result.push_back(ev);
      }
      break;
  }
  return result;
}
} // namespace rmlib::input
