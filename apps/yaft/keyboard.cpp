#include "keyboard.h"

#include <Device.h>

#include <unistd.h>

#include <algorithm>
#include <iostream>

using namespace rmlib;
using namespace rmlib::input;

namespace {
constexpr auto pen_slot = 0x1000;
constexpr char esc_char = '\x1b';

constexpr std::initializer_list<std::pair<std::string_view, std::string_view>>
  print_names = {
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

constexpr std::initializer_list<std::initializer_list<KeyInfo>> layout = {
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

static_assert(layout.size() == num_rows);
#if 0
static_assert(
  std::max_element(layout.begin(), layout.end(), [](auto& a, auto& b) {
    return a.size() < b.size();
  })->size() == row_size);
#endif

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
  }

  return nullptr;
}

} // namespace

Keyboard::Key::Key(const KeyInfo& info, rmlib::Rect rect)
  : info(info), keyRect(rect) {}

bool
Keyboard::init(rmlib::fb::FrameBuffer& fb, terminal_t& term) {
  this->term = &term;
  this->fb = &fb;
  baseKeyWidth = fb.canvas.width / row_size;
  startHeight = fb.canvas.height - keyboard_height;

  auto dev = device::getDeviceType();
  if (!dev.has_value()) {
    return false;
  }

  auto inputs = device::getInputPaths(*dev);
  const auto touchFd =
    input.open(inputs.touchPath.data(), inputs.touchTransform);
  if (!touchFd.has_value()) {
    return false;
  }

  const auto penFd = input.open(inputs.penPath.data(), inputs.penTransform);
  if (!penFd.has_value()) {
    return false;
  }

  // Setup the keymap.
  int y = startHeight;
  int rowNum = 0;
  for (const auto& row : layout) {
    int x = 0;
    for (const auto& key : row) {
      const auto keyWidth = baseKeyWidth * key.width;

      keys.emplace_back(
        key, Rect{ { x, y }, { x + keyWidth - 1, y + key_height - 1 } });

      // Store the modifier keys for later.
      if (key.code == Shift) {
        shiftKey = &keys.back();
      } else if (key.code == Alt) {
        altKey = &keys.back();
      } else if (key.code == Ctrl) {
        ctrlKey = &keys.back();
      }

      x += keyWidth;
    }

    y += key_height;
    rowNum++;
  }

  int marginLeft = term.width - term.cols * CELL_WIDTH;
  this->screenRect =
    Rect{ { marginLeft, term.marginTop }, { term.width - 1, term.height - 1 } };

  return true;
}

void
Keyboard::drawKey(const Key& key) const {
  const auto keyWidth = key.keyRect.width();

  fb->canvas.set(key.keyRect, 0xFF);

  fb->canvas.drawLine(
    key.keyRect.topLeft, key.keyRect.topLeft + Point{ keyWidth, 0 }, 0x0);
  fb->canvas.drawLine(
    key.keyRect.topLeft, key.keyRect.topLeft + Point{ 0, key_height }, 0x0);

  const auto printName = [&key = key.info] {
    if (key.name[0] != ':' || key.name.size() == 1) {
      return key.name;
    }
    const auto* it =
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
    fb->canvas.drawLine(a, { b.x, a.y }, 0x0);
    fb->canvas.drawLine(a, { a.x, b.y }, 0x0);
    fb->canvas.drawLine(b, { b.x, a.y }, 0x0);
    fb->canvas.drawLine(b, { a.x, b.y }, 0x0);
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
  bool shift = shiftKey->isDown();
  bool alt = altKey->isDown();
  bool ctrl = ctrlKey->isDown();

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

template<typename Event>
void
handleScreenEvent(Keyboard& kb, const Event& ev) {
  if ((kb.term->mode & MODE_MOUSE) == 0) {
    return;
  }

  const auto slot = event_traits<Event>::getSlot(ev);

  const auto loc = ev.location - kb.screenRect.topLeft;
  char cx = 33 + (loc.x / CELL_WIDTH);
  char cy = 33 + (loc.y / CELL_HEIGHT);
  char buf[] = { esc_char, '[', 'M', 32, cx, cy };

  if (ev.type == event_traits<Event>::down_type) {
    if (kb.mouseSlot != -1) {
      return;
    }

    kb.mouseSlot = slot;

    // Send mouse down code
    buf[3] += 0; // mouse button 1
    write(kb.term->fd, buf, 6);

  } else if (ev.type == event_traits<Event>::up_type) {
    if (kb.mouseSlot != slot) {
      return;
    }

    kb.mouseSlot = -1;

    // Send mouse up code
    buf[3] += 3; // mouse release
    write(kb.term->fd, buf, 6);
  }
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

    kb.drawKey(*key);
    kb.fb->doUpdate(
      key->keyRect, rmlib::fb::Waveform::DU, rmlib::fb::UpdateFlags::None);

    kb.sendKeyDown(*key);

    // Clear sticky keys.
    if (!isModifier(key->info.code)) {
      for (auto* key : { kb.shiftKey, kb.altKey, kb.ctrlKey }) {
        if (key->stuck) {
          key->stuck = false;
          key->nextRepeat = time_source::now() + repeat_delay;
          kb.drawKey(*key);
          kb.fb->doUpdate(key->keyRect,
                          rmlib::fb::Waveform::DU,
                          rmlib::fb::UpdateFlags::None);
        }
      }
    } else {
      if constexpr (std::is_same_v<Event, PenEvent>) {
        key->stuck = !key->stuck;
      } else {
        key->stuck = false;
      }
    }

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
} // namespace

void
Keyboard::handleEvents(const std::vector<rmlib::input::Event>& events) {
  for (const auto& event : events) {
    std::visit(
      [this](const auto& ev) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(ev)>, KeyEvent>) {
          if (screenRect.contains(ev.location)) {
            handleScreenEvent(*this, ev);
          } else {
            handleKeyEvent(*this, ev);
          }
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
        key.stuck = true;
      }

      sendKeyDown(key);
      key.nextRepeat += repeat_time;
    }
  }
}
