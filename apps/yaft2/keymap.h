#pragma once

#include <initializer_list>
#include <string_view>
#include <unordered_map>

struct EvKeyInfo {
  int scancode;

  int shfitScancode = 0; // Code when pressed with shift.

  int mod1Scancode = 0; // Left-alt scancode
  int mod2Scancode = 0; // Right-alt scancode

  // TODO: implement
  int holdCode = 0; // Key code when held.
};

using KeyMap = std::unordered_map<int, EvKeyInfo>;

extern const KeyMap qwerty_keymap;
extern const KeyMap rm_qwerty_keymap;
extern const KeyMap timower_keymap;

// Name mapping for config file
const std::initializer_list<std::pair<std::string_view, const KeyMap*>>
  keymaps = {
    { "qwerty", &qwerty_keymap },
    { "rm-qwerty", &rm_qwerty_keymap },
    { "timower", &timower_keymap },
  };
