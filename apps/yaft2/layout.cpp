#include "layout.h"

#include <algorithm>
#include <cassert>
#include <numeric>

std::size_t
Layout::numCols() const {
  const auto getSize = [](const auto& row) {
    return std::accumulate(
      row.begin(), row.end(), 0, [](auto total, const auto& key) {
        return total + key.width;
      });
  };

  assert([&] {
    if (rows.empty()) {
      return true;
    }
    const auto& firstRowSize = getSize(rows.front());
    return std::all_of(rows.begin(), rows.end(), [&](const auto& row) {
      return getSize(row) == firstRowSize;
    });
  }());

  return rows.empty() ? 0 : getSize(rows.front());
}

const Layout empty_layout = {};

const Layout qwerty_layout = { {
  { { "esc", Escape, "", 0, /* width */ 1, /* longPress */ makeCallback(1) },
    { ">", 0x3e, "<", 0x3c },
    { "|", 0x7c, "&", 0x26 },
    { "!", 0x21, "?", 0x3f },
    { "\"", 0x22, "$", 0x24 },
    { "'", 0x27, "`", 0x60 },
    { "_", 0x5f, "@", 0x40 },
    { "=", 0x3d, "~", 0x7e },
    { "/", 0x2f, "\\", 0x5c },
    { "*", 0x2a, "del", Del },
    { ":backspace", Backspace } },

  { { ":tab", Tab },
    { "1", 0x31, "+", 0x2b },
    { "2", 0x32, "^", 0x5e },
    { "3", 0x33, "#", 0x23 },
    { "4", 0x34, "%", 0x25 },
    { "5", 0x35, "(", 0x28 },
    { "6", 0x36, ")", 0x29 },
    { "7", 0x37, "[", 0x5b },
    { "8", 0x38, "]", 0x5d },
    { "9", 0x39, "{", 0x7b },
    { "0", 0x30, "}", 0x7d } },

  { { "q", 0x51 },
    { "w", 0x57 },
    { "e", 0x45 },
    { "r", 0x52 },
    { "t", 0x54 },
    { "y", 0x59 },
    { "u", 0x55 },
    { "i", 0x49 },
    { "o", 0x4f },
    { "p", 0x50 },
    { ":enter", Enter } },

  { { "a", 0x41 },
    { "s", 0x53 },
    { "d", 0x44 },
    { "f", 0x46 },
    { "g", 0x47 },
    { "h", 0x48 },
    { "j", 0x4a },
    { "k", 0x4b },
    { "l", 0x4c },
    { ":", 0x3a },
    { "pgup", PageUp, "home", Home } },

  { { ":shift", Shift },
    { "z", 0x5a },
    { "x", 0x58 },
    { "c", 0x43 },
    { "v", 0x56 },
    { "b", 0x42 },
    { "n", 0x4e },
    { "m", 0x4d },
    { ";", 0x3b },
    { ":up", Up },
    { "pgdn", PageDown, "end", End } },

  { { "ctrl", Ctrl },
    { "alt", Alt },
    { "-", 0x2d },
    { ",", 0x2c },
    { " ", 0x20, "", 0x0, /* width */ 3 },
    { ".", 0x2e },
    { ":left", Left },
    { ":down", Down },
    { ":right", Right } },
} };

const Layout hidden_layout = { { {
  { "esc", Escape, "", 0, /* width */ 1, /* longPress */ makeCallback(1) },
  { "pgup", PageUp },
  { "pgdn", PageDown },
  { "home", Home },
  { "end", End },
  { "<", Left },
  { "v", Down },
  { "^", Up },
  { ">", Right },
  { "ctrl-c", 0x3 },
  { "\\n", Enter },
} } };
