#include "keyboard.h"

#include "layout.h"

#include "yaft.h"

using namespace rmlib;

namespace {

constexpr auto pen_slot = 0x1000;
constexpr char esc_char = '\x1b';

const std::vector<std::pair<std::string_view, std::string_view>> print_names = {
  { ":backspace", "<==" }, { ":tab", "<=>" }, { ":enter", "\\n" },
  { ":shift", "shift" },   { ":up", "^" },    { ":down", "v" },
  { ":left", "<" },        { ":right", ">" },
};

constexpr bool
isModifier(int scancode) {
  return scancode == Shift || scancode == Ctrl || scancode == Alt;
}

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
  if (scancode == 0x3) {
    buf[0] = char(scancode);
    buf[1] = 0;
    return buf.data();
  }

  return nullptr;
}

template<typename Ev>
int
getSlot(const Ev& ev) {
  return ev.slot;
}

template<>
int
getSlot<input::PenEvent>(const input::PenEvent& ev) {
  return pen_slot;
}
} // namespace

std::unique_ptr<rmlib::RenderObject>
Keyboard::createRenderObject() const {
  return std::make_unique<KeyboardRenderObject>(*this);
}

KeyboardRenderObject::KeyboardRenderObject(const Keyboard& keyboard)
  : LeafRenderObject(keyboard) {
  updateLayout();
  markNeedsRebuild();
}

void
KeyboardRenderObject::update(const Keyboard& keyboard) {
  bool needsReLayout = &keyboard.params.layout != &widget->params.layout;
  widget = &keyboard;

  if (needsReLayout) {
    updateLayout();
    markNeedsLayout();
  }
}

void
KeyboardRenderObject::doRebuild(rmlib::AppContext& ctx,
                                const rmlib::BuildContext& /*buildContext*/) {
  const auto duration = getWidget().params.repeatTime;
  repeatTimer = ctx.addTimer(duration, [this]() { updateRepeat(); }, duration);
}

rmlib::Size
KeyboardRenderObject::doLayout(const rmlib::Constraints& constraints) {
  const auto numRows = widget->params.layout.numRows();
  const auto numCols = widget->params.layout.numCols();

  if (constraints.isBounded() && numRows != 0 && numCols != 0) {
    keyHeight = float(constraints.max.height) / numRows;
    keyWidth = float(constraints.max.width) / numCols;
    return constraints.max;
  }

  if (constraints.hasBoundedHeight() && numRows != 0) {
    keyHeight = float(constraints.max.height) / numRows;
    keyWidth = keyHeight / Keyboard::key_height * Keyboard::key_width;
    return Size{ int(keyWidth * numCols), int(keyHeight * numRows) };
  }

  if (constraints.hasBoundedWidth() && numCols != 0) {
    keyWidth = float(constraints.max.width) / numCols;
    keyHeight = keyWidth / Keyboard::key_width * Keyboard::key_height;
    return Size{ int(keyWidth * numCols), int(keyHeight * numRows) };
  }

  keyHeight = Keyboard::key_height;
  keyWidth = Keyboard::key_width;
  return constraints.min;
}

rmlib::UpdateRegion
KeyboardRenderObject::doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) {
  Point pos = rect.topLeft;

  UpdateRegion result;
  for (const auto& row : widget->params.layout.rows) {
    pos.x = rect.topLeft.x;

    for (const auto& key : row) {
      result |= drawKey(pos, key, canvas);

      pos.x += key.width * int(keyWidth);
    }

    pos.y += keyHeight;
  }

  if (!isPartialDraw()) {
    result.waveform = fb::Waveform::GC16Fast;
  }

  return result;
}

void
KeyboardRenderObject::handleInput(const rmlib::input::Event& ev) {
  std::visit(
    [this](const auto& ev) {
      if constexpr (rmlib::input::is_pointer_event<
                      std::decay_t<decltype(ev)>>) {
        if (getRect().contains(ev.location) || ev.isUp()) {
          handleTouchEvent(ev);
        }
      } else {
        handleKeyEvent(ev);
      }
    },
    ev);
}

void
KeyboardRenderObject::updateRepeat() {
  const auto time = TimeSource::now();
  const auto repeatTime = getWidget().params.repeatTime;

  for (auto& [key, state] : keyState) {
    if (state.slot == -1) {
      continue;
    }

    if (time > state.nextRepeat) {
      // If a modifier is long pressed, stick it.
      if (isModifier(key->code) && !state.held) {
        state.held = true;
        state.dirty = true;
        markNeedsDraw(/* full */ false);
      } else {
        sendKeyDown(*key, /* repeat */ true);
      }

      // Don't continue to repeat keys with special actions on longpress
      if (key->longPressCode == 0) {
        state.nextRepeat += repeatTime;
      } else {
        state.nextRepeat += std::chrono::hours(5);
      }
    }
  }

  for (auto& [key, state] : physKeyState) {
    if (!state.down) {
      continue;
    }

    if (time > state.nextRepeat) {
      if (!isModifier(key->scancode)) {
        sendKeyDown(*key, /* repeat */ true);
      }

      state.nextRepeat += repeatTime;
    }
  }
}

void
KeyboardRenderObject::updateLayout() {
  keyState.clear();
  physKeyState.clear();

  altKey = nullptr;
  ctrlKey = nullptr;
  shiftKey = nullptr;

  for (const auto& row : widget->params.layout.rows) {
    for (const auto& key : row) {
      if (key.code == ModifierKeys::Alt) {
        assert(altKey == nullptr);
        altKey = &key;
      } else if (key.code == ModifierKeys::Ctrl) {
        assert(ctrlKey == nullptr);
        ctrlKey = &key;
      } else if (key.code == ModifierKeys::Shift) {
        assert(shiftKey == nullptr);
        shiftKey = &key;
      }
    }
  }
}

void
KeyboardRenderObject::sendKeyDown(int scancode,
                                  bool shift,
                                  bool alt,
                                  bool ctrl) {
  if (isCallback(scancode)) {
    if (widget->callback) {
      widget->callback(getCallback(scancode));
    }
    return;
  }

  bool appCursor = (widget->term->mode & MODE_APP_CURSOR) != 0;
  const auto* code = getKeyCodeStr(scancode, shift, alt, ctrl, appCursor);
  if (code != nullptr) {
    write(widget->term->fd, code, strlen(code));
  } else {
    std::cerr << "unknown key combo: " << scancode << " shift " << shift
              << " ctrl " << ctrl << " alt " << alt << std::endl;
  }
}

void
KeyboardRenderObject::sendKeyDown(const KeyInfo& key, bool repeat) {
  if (isModifier(key.code)) {
    return;
  }

  // Lookup modifier state.
  bool shift = shiftKey == nullptr ? false : keyState[shiftKey].isDown();
  bool alt = altKey == nullptr ? false : keyState[altKey].isDown();
  bool ctrl = ctrlKey == nullptr ? false : keyState[ctrlKey].isDown();

  // scancode, use alt code if shift is pressed.
  auto scancode = (key.altCode != 0 && shift) ? key.altCode : key.code;
  if (repeat && key.longPressCode != 0) {
    scancode = key.longPressCode;
  }
  sendKeyDown(scancode, shift, alt, ctrl);
}
void
KeyboardRenderObject::sendKeyDown(const EvKeyInfo& key, bool repeat) {
  if (isModifier(key.scancode)) {
    return;
  }

  const auto anyKeyDown = [this](int scancode) {
    return std::any_of(physKeyState.begin(),
                       physKeyState.end(),
                       [scancode](const auto& codeAndKey) {
                         return codeAndKey.first->scancode == scancode &&
                                codeAndKey.second.down;
                       });
  };

  bool shift = anyKeyDown(Shift);
  bool alt = anyKeyDown(Alt);
  bool ctrl = anyKeyDown(Ctrl);
  bool mod1 = anyKeyDown(Mod1);
  bool mod2 = anyKeyDown(Mod2);

  auto scancode = [&] {
    if (key.mod1Scancode != 0 && mod1) {
      return key.mod1Scancode;
    }
    if (key.mod2Scancode != 0 && mod2) {
      return key.mod2Scancode;
    }
    if (key.shfitScancode != 0 && shift) {
      return key.shfitScancode;
    }

    return key.scancode;
  }();
  sendKeyDown(scancode, shift, alt, ctrl);
}

const KeyInfo*
KeyboardRenderObject::getKey(const rmlib::Point& point) {
  if (!getRect().contains(point)) {
    return nullptr;
  }

  const auto rowIdx = int((point.y - getRect().topLeft.y) / keyHeight);
  const auto columnIdx = int((point.x - getRect().topLeft.x) / keyWidth);
  const auto& row = widget->params.layout.rows[rowIdx];

  int keyCounter = 0;
  for (const auto& key : row) {
    if (keyCounter <= columnIdx && columnIdx < keyCounter + key.width) {
      return &key;
    }
    keyCounter += key.width;
  }
  return nullptr;
}

void
KeyboardRenderObject::clearSticky() {
  for (const auto* key : { shiftKey, altKey, ctrlKey }) {
    if (key == nullptr) {
      continue;
    }

    auto& state = keyState[key];

    // Prevent key being held when multi touch typing.
    state.nextRepeat = TimeSource::now() + getWidget().params.repeatDelay;

    if (state.stuck) {
      state.stuck = false;
      state.dirty = true;
    }
  }
}

template<typename Ev>
void
KeyboardRenderObject::handleTouchEvent(const Ev& ev) {
  if (ev.isDown()) {
    auto* key = getKey(ev.location);
    if (key == nullptr) {
      return;
    }

    sendKeyDown(*key);

    auto& state = keyState[key];
    state.dirty = true;
    state.slot = getSlot(ev);
    state.nextRepeat = TimeSource::now() + getWidget().params.repeatDelay;

    if (isModifier(key->code)) {
      if (!state.held) {
        state.stuck = !state.stuck;
      }
      state.held = false;
    } else {
      clearSticky();
    }

    markNeedsDraw(/* full */ false);
  } else if (ev.isUp()) {
    auto keyIt = std::find_if(
      keyState.begin(), keyState.end(), [slot = getSlot(ev)](const auto& pair) {
        return pair.second.slot == slot;
      });
    if (keyIt == keyState.end()) {
      return;
    }

    keyIt->second.dirty = true;
    keyIt->second.slot = -1;
    markNeedsDraw(/* full */ false);
  }
}

void
KeyboardRenderObject::handleKeyEvent(const rmlib::input::KeyEvent& ev) {
  const auto& keymap = widget->params.keymap;

  auto it = keymap.find(ev.keyCode);
  if (it == keymap.end()) {
    std::cout << "Unknown physical key: " << ev.keyCode << "\n";
    return;
  }
  const auto& key = it->second;
  auto& state = physKeyState[&key];

  if (ev.type == input::KeyEvent::Press) {
    state.down = true;
    state.nextRepeat = TimeSource::now() + getWidget().params.repeatDelay;
    sendKeyDown(key);
  } else if (ev.type == input::KeyEvent::Release) {
    state.down = false;
  }
}

rmlib::UpdateRegion
KeyboardRenderObject::drawKey(rmlib::Point pos,
                              const KeyInfo& key,
                              rmlib::Canvas& canvas) {
  auto& state = keyState[&key];
  if (isPartialDraw() && !state.dirty) {
    return {};
  }

  const auto keyWidth = int(this->keyWidth) * key.width;
  const auto keyHeight = int(this->keyHeight);
  const auto keyRect =
    Rect{ .topLeft = pos,
          .bottomRight = pos + Point{ keyWidth - 1, keyHeight - 1 } };

  canvas.set(keyRect, white);
  canvas.drawLine(pos, pos + Point{ keyWidth - 1, 0 }, black);
  canvas.drawLine(pos, pos + Point{ 0, keyHeight - 1 }, black);

  const auto printName = [&key] {
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

  const auto textSize = Canvas::getTextSize(printName, 32);
  canvas.drawText(
    printName,
    { keyRect.topLeft.x + (keyWidth / 2) - (textSize.width / 2),
      keyRect.topLeft.y + (keyHeight / 2) - (textSize.height / 2) },
    32);

  if (!key.altName.empty()) {
    const auto altTextSize = Canvas::getTextSize(key.altName, 26);
    canvas.drawText(key.altName,
                    { keyRect.topLeft.x + keyWidth - altTextSize.width - 4,
                      keyRect.topLeft.y + 3 },
                    26);
  }

  if (state.isDown()) {
    const auto a = keyRect.topLeft + Point{ 2, 2 };
    const auto b = keyRect.bottomRight - Point{ 1, 1 };
    canvas.drawLine(a, { a.x, b.y }, 0x0);
    canvas.drawLine(b, { b.x, a.y }, 0x0);

    if (state.held) {
      canvas.drawLine(a, { b.x, a.y }, 0x0);
      canvas.drawLine(b, { a.x, b.y }, 0x0);
    }
  }

  state.dirty = false;
  return { keyRect, fb::Waveform::DU, fb::UpdateFlags::Priority };
}
