#pragma once

#include <unordered_map>

struct EvKeyInfo {
  int scancode;
  int altscancode = 0; // Code when pressed with shift.
};

using KeyMap = std::unordered_map<int, EvKeyInfo>;

extern const KeyMap qwerty_keymap;
