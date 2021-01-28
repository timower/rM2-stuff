#pragma once

#include <Input.h>

#include <variant>

// DEPRECATED: will be replace by commands

struct GestureConfig {
  enum Type { Swipe, Pinch, Tap };
  enum Action { ShowApps, Launch, NextRunning, PrevRunning };

  Type type;
  std::variant<rmlib::input::PinchGesture::Direction,
               rmlib::input::SwipeGesture::Direction>
    direction;
  int fingers;

  Action action;
  std::string command;

  bool matches(const rmlib::input::SwipeGesture& g) const {
    return type == GestureConfig::Swipe &&
           std::get<rmlib::input::SwipeGesture::Direction>(direction) ==
             g.direction &&
           fingers == g.fingers;
  }

  bool matches(const rmlib::input::PinchGesture& g) const {
    return type == GestureConfig::Pinch &&
           std::get<rmlib::input::PinchGesture::Direction>(direction) ==
             g.direction &&
           fingers == g.fingers;
  }

  bool matches(const rmlib::input::TapGesture& g) const {
    return type == GestureConfig::Tap && g.fingers == fingers;
  }
};

struct Config {
  std::vector<GestureConfig> gestures;
};
