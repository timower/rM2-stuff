#pragma once

#include <Input.h>

#include <functional>
#include <variant>

struct ActionConfig {
  enum Type { Swipe, Pinch, Tap, Button };

  Type type;
  std::variant<rmlib::input::PinchGesture::Direction,
               rmlib::input::SwipeGesture::Direction>
    direction;
  int fingers;

  int keycode;

  bool matches(const rmlib::input::SwipeGesture& g) const {
    return type == ActionConfig::Swipe &&
           std::get<rmlib::input::SwipeGesture::Direction>(direction) ==
             g.direction &&
           fingers == g.fingers;
  }

  bool matches(const rmlib::input::PinchGesture& g) const {
    return type == ActionConfig::Pinch &&
           std::get<rmlib::input::PinchGesture::Direction>(direction) ==
             g.direction &&
           fingers == g.fingers;
  }

  bool matches(const rmlib::input::TapGesture& g) const {
    return type == ActionConfig::Tap && g.fingers == fingers;
  }

  bool matches(const rmlib::input::KeyEvent& ev) const {
    return type == ActionConfig::Button && ev.keyCode == keycode &&
           ev.type == rmlib::input::KeyEvent::Release;
  }
};

struct Action {
  ActionConfig config;
  std::function<void()> command;
};

struct Config {
  std::vector<Action> actions;
};
