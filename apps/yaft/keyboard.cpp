#include "keyboard.h"

#include "terminal.h"

#include <Device.h>

#include <unistd.h>

#include <algorithm>
#include <iostream>

#include <linux/input-event-codes.h>

using namespace rmlib;
using namespace rmlib::input;

namespace {
constexpr auto pen_slot = 0x1000;
constexpr char esc_char = '\x1b';

const std::vector<std::pair<std::string_view, std::string_view>> print_names = {
  { ":backspace", "<==" }, { ":tab", "<=>" }, { ":enter", "\\n" },
  { ":shift", "shift" },   { ":up", "^" },    { ":down", "v" },
  { ":left", "<" },        { ":right", ">" },
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
  PageDown

};

enum ModifierKeys {
  Shift = 0x2000000,
  Ctrl = 0x4000000,
  Alt = 0x8000000,
};

constexpr bool
isModifier(int scancode) {
  return scancode == Shift || scancode == Ctrl || scancode == Alt;
}

const std::vector<std::vector<KeyInfo>> layout = {
  { { "esc", Escape },
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
};

const std::vector<std::vector<KeyInfo>> hidden_layout = {
  { { "esc", Escape },
    { "pgup", PageUp },
    { "pgdn", PageDown },
    { "home", Home },
    { "end", End },
    { "<", Left },
    { "v", Down },
    { "^", Up },
    { ">", Right },
    { "ctrl-c", 0x3 },
    { "\\n", Enter } }
};

#if 0 // C++ 20
static_assert(layout.size() == num_rows);
static_assert(
  std::max_element(layout.begin(), layout.end(), [](auto& a, auto& b) {
    return a.size() < b.size();
  })->size() == row_size);
#endif

const std::vector<EvKeyInfo> keymap = {
  { KEY_ESC, Escape },
  { KEY_1, '1', '!' },
  { KEY_2, '2', '@' },
  { KEY_3, '3', '#' },
  { KEY_4, '4', '$' },
  { KEY_5, '5', '%' },
  { KEY_6, '6', '^' },
  { KEY_7, '7', '&' },
  { KEY_8, '8', '*' },
  { KEY_9, '9', '(' },
  { KEY_0, '0', ')' },
  { KEY_MINUS, '-', '_' },
  { KEY_EQUAL, '=', '+' },
  { KEY_BACKSPACE, Backspace },
  { KEY_TAB, Tab },
  { KEY_Q, 'Q' },
  { KEY_W, 'W' },
  { KEY_E, 'E' },
  { KEY_R, 'R' },
  { KEY_T, 'T' },
  { KEY_Y, 'Y' },
  { KEY_U, 'U' },
  { KEY_I, 'I' },
  { KEY_O, 'O' },
  { KEY_P, 'P' },
  { KEY_LEFTBRACE, '[', '{' },
  { KEY_RIGHTBRACE, ']', '}' },
  { KEY_ENTER, Enter },
  { KEY_LEFTCTRL, Ctrl },
  { KEY_A, 'A' },
  { KEY_S, 'S' },
  { KEY_D, 'D' },
  { KEY_F, 'F' },
  { KEY_G, 'G' },
  { KEY_H, 'H' },
  { KEY_J, 'J' },
  { KEY_K, 'K' },
  { KEY_L, 'L' },
  { KEY_SEMICOLON, ';', ':' },
  { KEY_APOSTROPHE, '\'', '"' },
  { KEY_GRAVE, '`', '~' },
  { KEY_LEFTSHIFT, Shift },
  { KEY_BACKSLASH, '\\', '|' },
  { KEY_Z, 'Z' },
  { KEY_X, 'X' },
  { KEY_C, 'C' },
  { KEY_V, 'V' },
  { KEY_B, 'B' },
  { KEY_N, 'N' },
  { KEY_M, 'M' },
  { KEY_COMMA, ',', '<' },
  { KEY_DOT, '.', '>' },
  { KEY_SLASH, '/', '?' },
  { KEY_RIGHTSHIFT, Shift },
  { KEY_KPASTERISK, '*' },
  { KEY_LEFTALT, Alt },
  { KEY_SPACE, ' ' },
  // { KEY_CAPSLOCK
  // { KEY_F1 59
  // { KEY_F2 60
  // { KEY_F3 61
  // { KEY_F4 62
  // { KEY_F5 63
  // { KEY_F6 64
  // { KEY_F7 65
  // { KEY_F8 66
  // { KEY_F9 67
  // { KEY_F10 68
  // { KEY_NUMLOCK 69
  // { KEY_SCROLLLOCK 70
  { KEY_KP7, '7' },
  { KEY_KP8, '8' },
  { KEY_KP9, '9' },
  { KEY_KPMINUS, '-' },
  { KEY_KP4, '4' },
  { KEY_KP5, '5' },
  { KEY_KP6, '6' },
  { KEY_KPPLUS, '+' },
  { KEY_KP1, '1' },
  { KEY_KP2, '2' },
  { KEY_KP3, '3' },
  { KEY_KP0, '0' },
  { KEY_KPDOT, '.' },

  // { KEY_ZENKAKUHANKAKU 85
  // { KEY_102ND 86
  // { KEY_F11 87
  // { KEY_F12 88
  // { KEY_RO 89
  // { KEY_KATAKANA 90
  // { KEY_HIRAGANA 91
  // { KEY_HENKAN 92
  // { KEY_KATAKANAHIRAGANA 93
  // { KEY_MUHENKAN 94
  // { KEY_KPJPCOMMA 95
  { KEY_KPENTER, Enter },
  { KEY_RIGHTCTRL, Ctrl },
  { KEY_KPSLASH, '/' },
  // { KEY_SYSRQ 99
  { KEY_RIGHTALT, Alt },
  { KEY_LINEFEED, Enter },
  { KEY_HOME, Home },
  { KEY_UP, Up },
  { KEY_PAGEUP, PageUp },
  { KEY_LEFT, Left },
  { KEY_RIGHT, Right },
  { KEY_END, End },
  { KEY_DOWN, Down },
  { KEY_PAGEDOWN, PageDown },
  // { KEY_INSERT 110
  { KEY_DELETE, Del },
  // { KEY_MACRO 112
  // { KEY_MUTE 113
  // { KEY_VOLUMEDOWN 114
  // { KEY_VOLUMEUP 115
  // { KEY_POWER 116 /* SC System Power Down */
  { KEY_KPEQUAL, '=' },
  { KEY_KPPLUSMINUS, '+', '-' },
  // { KEY_PAUSE 119
  // { KEY_SCALE 120 /* AL Compiz Scale (Expose) */

};

const char*
getKeyCodeStr(int scancode, bool shift, bool alt, bool ctrl, bool appCursor) {
  static std::array<char, 512> buf;

  constexpr auto write_vt_code = [](char code) {
    buf[0] = esc_char;
    buf[1] = '[';
    buf[2] = code;
    buf[3] = '~';
    buf[4] = 0;
  };

  constexpr auto write_xt_code = [](char code, bool appCursor) {
    buf[0] = esc_char;
    buf[1] = appCursor ? 'O' : '[';
    buf[2] = code;
    buf[3] = 0;
  };

  if (scancode > 0xFF) {
    // TODO: support modifiers.
    switch (scancode) {
      case Escape:
        buf[0] = esc_char;
        buf[1] = 0;
        return buf.data();
      case Tab:
        buf[0] = '\t';
        buf[1] = 0;
        return buf.data();
      case Backspace:
        buf[0] = '\x7f';
        buf[1] = 0;
        return buf.data();
      case Enter:
        buf[0] = '\r'; // TODO: support newline mode
        buf[1] = 0;
        return buf.data();
      case Del:
        write_vt_code('3');
        return buf.data();
      case Home:
        write_vt_code('1');
        return buf.data();
      case End:
        write_vt_code('4');
        return buf.data();

      case Left:
        write_xt_code('D', appCursor);
        return buf.data();
      case Up:
        write_xt_code('A', appCursor);
        return buf.data();
      case Right:
        write_xt_code('C', appCursor);
        return buf.data();
      case Down:
        write_xt_code('B', appCursor);
        return buf.data();

      case PageUp:
        write_vt_code('5');
        return buf.data();
      case PageDown:
        write_vt_code('6');
        return buf.data();
    }

    return nullptr;
  }

  if (isprint(scancode) != 0) {
    if (ctrl) {
      if (0x41 <= scancode && scancode <= 0x5f) {
        scancode -= 0x40;
      } else {
        return nullptr;
      }
    }

    if (isalpha(scancode) != 0 && !shift) {
      scancode = tolower(scancode);
    }

    if (alt) {
      buf[0] = esc_char;
      buf[1] = char(scancode);
      buf[2] = 0;
    } else {
      buf[0] = char(scancode);
      buf[1] = 0;
    }
    return buf.data();
  } else if (scancode == 0x3) {
    buf[0] = char(scancode);
    buf[1] = 0;
    return buf.data();
  }

  return nullptr;
}

} // namespace

Keyboard::Key::Key(const KeyInfo& info, rmlib::Rect rect)
  : info(info), keyRect(rect) {}

OptError<>
Keyboard::init(rmlib::fb::FrameBuffer& fb, terminal_t& term) {
  this->term = &term;
  this->fb = &fb;

  TRY(input.openAll());

  initKeyMap();
  return NoError{};
}

void
Keyboard::initKeyMap() {
  baseKeyWidth = fb->canvas.width / row_size;
  startHeight =
    fb->canvas.height - (hidden ? hidden_keyboard_height : keyboard_height);

  // Resize the terminal to make place for the keyboard.
  term_resize(term, fb->canvas.width, startHeight);

  // Setup the keymap.
  keys.clear();

  shiftKey = -1;
  altKey = -1;
  ctrlKey = -1;

  int y = startHeight;
  int rowNum = 0;

  const auto& currentLayout = hidden ? hidden_layout : layout;

  for (const auto& row : currentLayout) {
    int x = (fb->canvas.width - row_size * baseKeyWidth) / 2;
    for (const auto& key : row) {
      const auto keyWidth = baseKeyWidth * key.width;

      // Store the modifier keys for later.
      if (key.code == Shift) {
        shiftKey = keys.size();
      } else if (key.code == Alt) {
        altKey = keys.size();
      } else if (key.code == Ctrl) {
        ctrlKey = keys.size();
      }

      keys.emplace_back(
        key, Rect{ { x, y }, { x + keyWidth - 1, y + key_height - 1 } });

      x += keyWidth;
    }

    y += key_height;
    rowNum++;
  }

  int marginLeft = term->width - term->cols * CELL_WIDTH;
  this->screenRect = Rect{ { marginLeft, term->marginTop },
                           { term->width - 1, term->height - 1 } };

  // do physical keys:
  for (const auto& keyInfo : keymap) {
    physicalKeys.emplace(keyInfo.code, PhysicalKey{ keyInfo });
  }
}

void
Keyboard::drawKey(const Key& key) const {
  const auto keyWidth = key.keyRect.width();

  fb->canvas.set(key.keyRect, 0xFF);

  fb->canvas.drawLine(
    key.keyRect.topLeft, key.keyRect.topLeft + Point{ keyWidth - 1, 0 }, 0x0);
  fb->canvas.drawLine(
    key.keyRect.topLeft, key.keyRect.topLeft + Point{ 0, key_height - 1 }, 0x0);

  const auto printName = [&key = key.info] {
    if (key.name[0] != ':' || key.name.size() == 1) {
      return key.name;
    }
    const auto it =
      std::find_if(print_names.begin(),
                   print_names.end(),
                   [&key](const auto& pair) { return pair.first == key.name; });
    if (it == print_names.end()) {
      return std::string_view("?");
    }
    return it->second;
  }();

  auto textSize = Canvas::getTextSize(printName, 32);
  fb->canvas.drawText(
    printName,
    { key.keyRect.topLeft.x + keyWidth / 2 - textSize.x / 2,
      key.keyRect.topLeft.y + key_height / 2 - textSize.y / 2 },
    32);

  if (!key.info.altName.empty()) {
    auto altTextSize = Canvas::getTextSize(key.info.altName, 26);
    fb->canvas.drawText(key.info.altName,
                        { key.keyRect.topLeft.x + keyWidth - altTextSize.x - 4,
                          key.keyRect.topLeft.y + 3 },
                        26);
  }

  // draw extra border
  if (key.isDown()) {
    auto a = key.keyRect.topLeft + Point{ 2, 2 };
    auto b = key.keyRect.bottomRight - Point{ 1, 1 };
    fb->canvas.drawLine(a, { a.x, b.y }, 0x0);
    fb->canvas.drawLine(b, { b.x, a.y }, 0x0);

    if (key.held) {
      fb->canvas.drawLine(a, { b.x, a.y }, 0x0);
      fb->canvas.drawLine(b, { a.x, b.y }, 0x0);
    }
  }
}

void
Keyboard::draw() const {
  Rect keyboardRect = { { 0, startHeight },
                        { fb->canvas.width - 1, fb->canvas.height - 1 } };

  for (const auto& key : keys) {
    drawKey(key);
  }

  fb->doUpdate(keyboardRect, fb::Waveform::GC16Fast, fb::UpdateFlags::None);
}

Keyboard::Key*
Keyboard::getKey(Point location) {

  for (auto& key : keys) {
    if (key.keyRect.contains(location)) {
      return &key;
    }
  }

  return nullptr;
}

void
Keyboard::sendKeyDown(const Key& key) const {

  // No code for modifiers.
  if (isModifier(key.info.code)) {
    return;
  }

  // Lookup modifier state.
  bool shift = shiftKey == -1 ? false : keys.at(shiftKey).isDown();
  bool alt = altKey == -1 ? false : keys.at(altKey).isDown();
  bool ctrl = ctrlKey == -1 ? false : keys.at(ctrlKey).isDown();

  bool appCursor = (term->mode & MODE_APP_CURSOR) != 0;

  // scancode, use alt code if shift is pressed.
  auto scancode =
    (key.info.altCode != 0 && shift) ? key.info.altCode : key.info.code;

  const auto* code = getKeyCodeStr(scancode, shift, alt, ctrl, appCursor);
  if (code != nullptr) {
    write(term->fd, code, strlen(code));
  } else {
    std::cerr << "unknown key combo: " << scancode << " shift " << shift
              << " ctrl " << ctrl << " alt " << alt << std::endl;
  }
}

void
Keyboard::sendKeyDown(const PhysicalKey& key) const {
  if (isModifier(key.info.scancode)) {
    return;
  }

  const auto anyKeyDown = [this](int scancode) {
    return std::any_of(physicalKeys.begin(),
                       physicalKeys.end(),
                       [scancode](const auto& codeAndKey) {
                         return codeAndKey.second.info.scancode == scancode &&
                                codeAndKey.second.down;
                       });
  };

  bool shift = anyKeyDown(Shift);
  bool alt = anyKeyDown(Alt);
  bool ctrl = anyKeyDown(Ctrl);

  bool appCursor = (term->mode & MODE_APP_CURSOR) != 0;

  auto scancode = (key.info.altscancode != 0 && shift) ? key.info.altscancode
                                                       : key.info.scancode;

  const auto* code = getKeyCodeStr(scancode, shift, alt, ctrl, appCursor);
  if (code != nullptr) {
    write(term->fd, code, strlen(code));
  } else {
    std::cerr << "unknown key combo: " << scancode << " shift " << shift
              << " ctrl " << ctrl << " alt " << alt << std::endl;
  }
}

namespace {
template<typename Event>
struct event_traits;

template<>
struct event_traits<TouchEvent> {
  constexpr static auto up_type = TouchEvent::Up;
  constexpr static auto down_type = TouchEvent::Down;

  constexpr static auto getSlot(const TouchEvent& ev) { return ev.slot; }
};

template<>
struct event_traits<PenEvent> {
  constexpr static auto up_type = PenEvent::TouchUp;
  constexpr static auto down_type = PenEvent::TouchDown;

  constexpr static auto getSlot(const PenEvent& ev) { return pen_slot; }
};

void
initMouseBuf(std::array<char, 6>& buf, Point loc) {
  loc.x /= CELL_WIDTH;
  loc.y /= CELL_HEIGHT;

  char cx = 33 + loc.x;
  char cy = 33 + loc.y;
  buf = { esc_char, '[', 'M', 32, cx, cy };
}

// TODO: gesture progress updates, don't do multiple scrolls
void
handleGesture(Keyboard& kb, const SwipeGesture& gesture) {
  if (gesture.fingers != 2) {
    return;
  }

  std::array<char, 6> buf;
  const auto loc = gesture.startPosition - kb.screenRect.topLeft;
  initMouseBuf(buf, loc);

  constexpr auto scroll_size = 4 * CELL_HEIGHT;
  const auto distance = std::max(
    1, abs(gesture.endPosition.y - gesture.startPosition.y) / scroll_size);

  if (gesture.direction == rmlib::input::SwipeGesture::Up) {
    buf[3] += 64 + 1;
    for (int i = 0; i < distance; i++) {
      write(kb.term->fd, buf.data(), buf.size());
    }
  } else if (gesture.direction == rmlib::input::SwipeGesture::Down) {
    buf[3] += 64 + 0;
    for (int i = 0; i < distance; i++) {
      write(kb.term->fd, buf.data(), buf.size());
    }
  }
}

template<typename Event>
void
handleScreenEvent(Keyboard& kb, const Event& ev) {
  if ((kb.term->mode & ALL_MOUSE_MODES) == 0) {
    return;
  }

  const auto lastFingers = kb.gestureCtrlr.getCurrentFingers();

  if constexpr (std::is_same_v<Event, TouchEvent>) {
    const auto [gestures, _] = kb.gestureCtrlr.handleEvents({ ev });
    for (const auto& gesture : gestures) {
      if (std::holds_alternative<SwipeGesture>(gesture)) {
        handleGesture(kb, std::get<SwipeGesture>(gesture));
      }
    }
  }

  const auto slot = event_traits<Event>::getSlot(ev);
  const auto loc = ev.location - kb.screenRect.topLeft;
  std::array<char, 6> buf;
  initMouseBuf(buf, loc);

  // Mouse down on first finger if mouse is not down.
  if (ev.type == event_traits<Event>::down_type && kb.mouseSlot == -1 &&
      lastFingers == 0) {
    kb.mouseSlot = slot;

    // Send mouse down code
    buf[3] += 0; // mouse button 1
    write(kb.term->fd, buf.data(), buf.size());

  } else if ((ev.type == event_traits<Event>::up_type &&
              slot == kb.mouseSlot) ||
             (kb.mouseSlot != -1 && kb.gestureCtrlr.getCurrentFingers() > 1)) {
    // Mouse up after finger lift or second finger down (for scrolling).

    kb.mouseSlot = -1;

    // Send mouse up code
    buf[3] += 3; // mouse release
    write(kb.term->fd, buf.data(), buf.size());
  } else if (kb.mouseSlot == slot && kb.lastMousePos != loc &&
             (kb.term->mode & MODE_MOUSE_MOVE) != 0) {
    buf[3] += 32; // mouse move
    write(kb.term->fd, buf.data(), buf.size());
  }
  kb.lastMousePos = loc;
}

template<typename Event>
void
handleKeyEvent(Keyboard& kb, const Event& ev) {
  if (ev.type == event_traits<Event>::down_type) {
    // lookup key, skip if outside
    auto* key = kb.getKey(ev.location);
    if (key == nullptr) {
      return;
    }

    std::cerr << "key down: " << key->info.name << std::endl;

    key->slot = event_traits<Event>::getSlot(ev);
    key->nextRepeat = time_source::now() + repeat_delay;

    kb.sendKeyDown(*key);

    // Clear sticky keys.
    if (!isModifier(key->info.code)) {
      for (auto keyIdx : { kb.shiftKey, kb.altKey, kb.ctrlKey }) {
        if (keyIdx == -1) {
          continue;
        }

        auto& key = kb.keys[keyIdx];

        key.nextRepeat = time_source::now() + repeat_delay;
        if (key.stuck) {
          key.stuck = false;
          kb.drawKey(key);
          kb.fb->doUpdate(
            key.keyRect, rmlib::fb::Waveform::DU, rmlib::fb::UpdateFlags::None);
        }
      }
    } else {
      if (!key->held) {
        key->stuck = !key->stuck;
      }
      key->held = false;
    }

    kb.drawKey(*key);
    kb.fb->doUpdate(
      key->keyRect, rmlib::fb::Waveform::DU, rmlib::fb::UpdateFlags::None);

  } else if (ev.type == event_traits<Event>::up_type) {
    const auto slot = event_traits<Event>::getSlot(ev);
    auto keyIt =
      std::find_if(kb.keys.begin(), kb.keys.end(), [slot](const auto& key) {
        return key.slot == slot;
      });

    if (keyIt == kb.keys.end()) {
      return;
    }

    keyIt->slot = -1;
    kb.drawKey(*keyIt);
    kb.fb->doUpdate(
      keyIt->keyRect, rmlib::fb::Waveform::DU, rmlib::fb::UpdateFlags::None);
  }
}

template<typename Ev>
bool
isKeyRelease(const Keyboard& kb, const Ev& ev) {
  if (event_traits<Ev>::up_type != ev.type) {
    return false;
  }

  auto slot = event_traits<Ev>::getSlot(ev);
  return std::any_of(kb.keys.begin(), kb.keys.end(), [slot](const auto& key) {
    return key.slot == slot;
  });
}

void
handlePhysicalKeyEvent(Keyboard& kb, const KeyEvent& ev) {
  auto it = kb.physicalKeys.find(ev.keyCode);
  if (it == kb.physicalKeys.end()) {
    std::cout << "Unknown physical key: " << ev.keyCode << "\n";
    return;
  }
  auto& key = it->second;

  if (ev.type == KeyEvent::Press) {
    key.down = true;
    key.nextRepeat = time_source::now() + repeat_delay;
    kb.sendKeyDown(key);
  } else if (ev.type == KeyEvent::Release) {
    key.down = false;
  }
}

} // namespace

void
Keyboard::handleEvents(const std::vector<rmlib::input::Event>& events) {
  for (const auto& event : events) {
    std::visit(
      [this](const auto& ev) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(ev)>, KeyEvent>) {

          // If the event is not on the screen or it's a release event of a
          // previously pressed key, handle it as a keyboard event. Otherwise
          // handle it as a screen (mouse) event.
          if (!screenRect.contains(ev.location) || isKeyRelease(*this, ev)) {
            handleKeyEvent(*this, ev);
          } else {
            handleScreenEvent(*this, ev);
          }
        } else {
          handlePhysicalKeyEvent(*this, ev);
        }
      },
      event);
  }
}

void
Keyboard::updateRepeat() {
  const auto time = time_source::now();

  for (auto& key : keys) {
    if (key.slot == -1) {
      continue;
    }

    if (time > key.nextRepeat) {
      // If a modifier is long pressed, stick it.
      if (isModifier(key.info.code)) {
        key.held = true;

        drawKey(key);
        fb->doUpdate(
          key.keyRect, rmlib::fb::Waveform::DU, rmlib::fb::UpdateFlags::None);

      } else if (key.info.code == Escape) {

        if (hidden) {
          show();
        } else {
          hide();
        }
        break;

      } else {
        sendKeyDown(key);
      }

      key.nextRepeat += repeat_time;
    }
  }

  for (auto& [_, key] : physicalKeys) {
    (void)_;

    if (!key.down) {
      continue;
    }

    if (time > key.nextRepeat) {
      if (!isModifier(key.info.scancode)) {
        sendKeyDown(key);
      }

      key.nextRepeat += repeat_time;
    }
  }
}

void
Keyboard::hide() {
  if (hidden) {
    return;
  }

  initKeyMap();
  draw();

  hidden = true;
}

void
Keyboard::show() {
  if (!hidden) {
    return;
  }

  initKeyMap();
  draw();

  hidden = false;
}
