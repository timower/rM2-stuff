#include "Input.h"

#include <atomic>
#include <iostream>
#include <thread>

#include <sys/select.h>
#include <unistd.h>

#include <SDL.h>

namespace rmlib::input {

void
InputDeviceBase::grab() {}

void
InputDeviceBase::ungrab() {}

InputDeviceBase::~InputDeviceBase() {}

namespace {
struct FakeInputDevice : public InputDeviceBase {
  int pipes[2];
  unsigned int sdlUserEvent;

  FakeInputDevice() : InputDeviceBase(0, nullptr, "test") {
    sdlUserEvent = SDL_RegisterEvents(1);

    int res = pipe(pipes);
    if (res != 0) {
      perror("Error creating input notify pipes");
    }
    std::cout << "Got pipes: " << pipes[0] << ", " << pipes[1] << "\n";
  }

  void flood() final {}
  OptError<> readEvents(std::vector<Event>& out) final { return {}; }
};
} // namespace

InputManager::InputManager() {}

InputManager::~InputManager() {}

ErrorOr<BaseDevices>
InputManager::openAll(bool monitor) {
  if (SDL_Init(SDL_INIT_EVENTS) < 0) {
    return Error::make(std::string("could not initialize sdl2:") +
                       SDL_GetError());
  }

  auto fakeDev = std::make_unique<FakeInputDevice>();
  auto& fakeDevRef = *fakeDev;
  devices.emplace("Test", std::move(fakeDev));

  baseDevices.emplace(BaseDevices{ fakeDevRef, fakeDevRef, fakeDevRef });
  return *baseDevices;
}

ErrorOr<std::vector<Event>>
InputManager::waitForInput(fd_set& fdSet,
                           int maxFd,
                           std::optional<std::chrono::microseconds> timeout) {
  static bool down = false;

  FakeInputDevice& dev = static_cast<FakeInputDevice&>(getBaseDevices()->key);

  std::thread selectThread([maxFd,
                            readPipe = dev.pipes[0],
                            &fdSet,
                            &timeout,
                            userEv = dev.sdlUserEvent]() {
    if (maxFd == 0 && !FD_ISSET(0, &fdSet)) {
      return;
    }

    // Also listen to the notify pipe
    int maxFd2 = std::max(maxFd, readPipe);
    FD_SET(readPipe, &fdSet);

    auto tv = timeval{ 0, 0 };
    if (timeout.has_value()) {
      constexpr auto second_in_usec =
        std::chrono::microseconds(std::chrono::seconds(1)).count();
      tv.tv_sec =
        std::chrono::duration_cast<std::chrono::seconds>(*timeout).count();
      tv.tv_usec = timeout->count() - (tv.tv_sec * second_in_usec);
    }

    auto ret = select(maxFd2 + 1,
                      &fdSet,
                      nullptr,
                      nullptr,
                      timeout.has_value() ? &tv : nullptr);

    if (ret < 0) {
      perror("Select on input failed");
      return;
    }

    if (ret == 0) {
      return;
    }

    // Read the single notify byte.
    if (FD_ISSET(readPipe, &fdSet)) {
      char buf;
      read(readPipe, &buf, 1);
      return;
    }

    SDL_Event event;
    event.type = userEv;
    SDL_PushEvent(&event);
  });

  int sdlRes = 1;
  SDL_Event event;
  if (timeout.has_value()) {
    SDL_ClearError();
    sdlRes = SDL_WaitEventTimeout(&event, timeout->count() / 1000);
  } else {
    sdlRes = SDL_WaitEvent(&event);
  }

  if (const auto* err = SDL_GetError(); sdlRes == 0 && *err != 0) {
    std::cerr << "SDL error: " << err << "\n";
  }

  if (event.type != dev.sdlUserEvent) {
    write(dev.pipes[1], "1", 1);
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
      return Error::make("stop");
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
