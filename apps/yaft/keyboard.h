#pragma once

#include "yaft.h"
#include <FrameBuffer.h>
#include <Input.h>

#include <chrono>

constexpr int row_size = 11;
constexpr int num_rows = 6;

// key width = screen width / row_size
// key height = ??
constexpr int key_height = 64;
constexpr int keyboard_height = key_height * num_rows;

struct KeyInfo {
  std::string_view name;
  int code;

  std::string_view altName = "";
  int altCode = 0;

  int width = 1;
};

using time_source = std::chrono::steady_clock;

constexpr auto repeat_delay = std::chrono::seconds(1);
constexpr auto repeat_time = std::chrono::milliseconds(100);

struct Keyboard {
  struct Key {
    Key(const KeyInfo& info, rmlib::Rect rect);
    const KeyInfo& info;
    rmlib::Rect keyRect;

    int slot = -1;
    bool stuck = false;

    bool isDown() const { return slot != -1 || stuck; }

    time_source::time_point nextRepeat;
  };

  bool init(rmlib::fb::FrameBuffer& fb, terminal_t& term);

  void draw() const;

  void readEvents();
  void updateRepeat();

  void drawKey(const Key& key) const;

  Key* getKey(rmlib::Point location);

  void sendKeyDown(const Key& key);

  // members
  int baseKeyWidth;
  int startHeight;

  int touchFd;
  rmlib::input::InputManager input;
  rmlib::fb::FrameBuffer* fb;
  terminal_t* term;

  // Slots for tracking modifier state.
  Key* shiftKey = nullptr;
  Key* altKey = nullptr;
  Key* ctrlKey = nullptr;

  std::vector<Key> keys;
};
