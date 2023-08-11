#pragma once

#include <string_view>
#include <vector>

struct KeyInfo {
  std::string_view name;
  int code;

  std::string_view altName = "";
  int altCode = 0;

  int width = 1;

  int longPressCode = 0;
};

struct Layout {
  std::vector<std::vector<KeyInfo>> rows;

  std::size_t numRows() const { return rows.size(); }
  std::size_t numCols() const;
};

enum SpecialKeys {
  Escape = 0x1000000,
  Tab,
  Backspace,
  Enter,
  Del,
  Home,
  End,

  Left,
  Up,
  Right,
  Down,

  PageUp,
  PageDown,

  Mod1,
  Mod2,

  Callback = 0x1100000,
};

inline bool
isCallback(int code) {
  return (code & Callback) == Callback;
}

inline int
getCallback(int code) {
  return code & 0x00fffff;
}

inline int
makeCallback(int num) {
  return num | Callback;
}

enum ModifierKeys {
  Shift = 0x2000000,
  Ctrl = 0x4000000,
  Alt = 0x8000000,
};

extern const Layout qwerty_layout;
extern const Layout hidden_layout;
extern const Layout empty_layout;

const std::initializer_list<std::pair<std::string_view, const Layout*>>
  layouts = {
    { "qwerty", &qwerty_layout },
  };
