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
constexpr int hidden_keyboard_height = key_height;

struct KeyInfo {
  std::string_view name;
  int code;

  std::string_view altName = "";
  int altCode = 0;

  int width = 1;
};

struct EvKeyInfo {
  int code;
  int scancode;
  int altscancode = 0; // Code when pressed with shift.
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

    bool stuck = false; // Used for mod keys, get stuck after tap.
    bool held = false;  // Used for mod keys, held down after long press.

    bool isDown() const { return slot != -1 || stuck || held; }

    time_source::time_point nextRepeat;
  };

  struct PhysicalKey {
    const EvKeyInfo& info;
    bool down = false;

    time_source::time_point nextRepeat = {};
  };

  OptError<> init(rmlib::fb::FrameBuffer& fb, terminal_t& term);
  void initKeyMap();

  void draw() const;

  void handleEvents(const std::vector<rmlib::input::Event>& events);
  void updateRepeat();

  void drawKey(const Key& key) const;

  Key* getKey(rmlib::Point location);

  void sendKeyDown(const Key& key) const;
  void sendKeyDown(const PhysicalKey& key) const;

  void hide();
  void show();

  // members
  int baseKeyWidth;
  int startHeight;
  rmlib::Rect screenRect;

  rmlib::input::InputManager input;
  rmlib::input::GestureController gestureCtrlr;
  rmlib::fb::FrameBuffer* fb;
  terminal_t* term;

  int mouseSlot = -1;
  rmlib::Point lastMousePos;

  // Pointers for tracking modifier state.
  int shiftKey = -1;
  int altKey = -1;
  int ctrlKey = -1;

  std::vector<Key> keys;

  // Map from evdev key code to PhysicalKey.
  std::unordered_map<int, PhysicalKey> physicalKeys;

  bool hidden = false;
};
