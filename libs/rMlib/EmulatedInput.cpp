#include "Input.h"

#include <unistdpp/pipe.h>

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
  unistdpp::Pipe pipes;
  unsigned int sdlUserEvent;

  FakeInputDevice() : InputDeviceBase(unistdpp::FD(), nullptr, "test") {
    sdlUserEvent = SDL_RegisterEvents(1);

    pipes = unistdpp::pipe()
              .or_else([](auto err) {
                std::cerr << unistdpp::toString(err) << "\n";
                std::exit(EXIT_FAILURE);
              })
              .value();
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
InputManager::waitForInput(std::vector<pollfd>& extraFds,
                           std::optional<std::chrono::milliseconds> timeout) {
  static bool down = false;

  FakeInputDevice& dev = static_cast<FakeInputDevice&>(getBaseDevices()->key);

  std::thread selectThread([&readPipe = dev.pipes.readPipe,
                            &extraFds,
                            &timeout,
                            userEv = dev.sdlUserEvent]() {
    // Also listen to the notify pipe
    extraFds.push_back(unistdpp::waitFor(readPipe, unistdpp::Wait::READ));

    auto ret = unistdpp::poll(extraFds, timeout);
    if (!ret) {
      std::cerr << "Poll failure: " << unistdpp::toString(ret.error()) << "\n";
      return;
    }

    if (ret == 0) {
      return;
    }

    // Remove the added Fd
    auto addedFd = extraFds.back();
    extraFds.pop_back();

    // Read the single notify byte.
    if (unistdpp::canRead(addedFd)) {
      (void)readPipe.readAll<char>();
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
    (void)dev.pipes.writePipe.writeAll('1');
  }

  selectThread.join();

  KeyEvent keyEv;

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
        auto x = event.button.x * EMULATE_SCALE;
        auto y = event.button.y * EMULATE_SCALE;
        down = true;
        ev.type = TouchEvent::Down;
        ev.location = { x, y };
        result.push_back(ev);
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        auto x = event.button.x * EMULATE_SCALE;
        auto y = event.button.y * EMULATE_SCALE;
        down = false;
        ev.type = TouchEvent::Up;
        ev.location = { x, y };
        result.push_back(ev);
      }
      break;

    case SDL_KEYDOWN:
      keyEv.keyCode = event.key.keysym.scancode;
      keyEv.type = KeyEvent::Press;
      result.push_back(keyEv);
      break;
    case SDL_KEYUP:
      keyEv.keyCode = event.key.keysym.scancode;
      keyEv.type = KeyEvent::Release;
      result.push_back(keyEv);
      break;
  }
  return result;
}
} // namespace rmlib::input
