#include "KeyPad.h"

#include "scancodes.h"

using namespace rmlib;
using namespace rmlib::input;

namespace tilem {
struct Key {
  int scancode;
  std::string_view front;
  std::string_view shift;
  std::string_view alpha;
  float width = 1;
};

namespace {

constexpr auto shift_color = 0x55;
constexpr auto alpha_color = 0xaa;

const std::vector<std::vector<Key>> keymap = {
  { { TILEM_KEY_YEQU, "Y=", "STAT PLOT", "F1" },
    { TILEM_KEY_WINDOW, "WINDOW", "TBLST", "F2" },
    { TILEM_KEY_ZOOM, "ZOOM", "FORMAT", "F3" },
    { TILEM_KEY_TRACE, "TRACE", "CALC", "F4" },
    { TILEM_KEY_GRAPH, "GRAPH", "TABLE", "F5" } },

  { { 0, "", "", "", 3.5F }, { TILEM_KEY_UP, "Λ", "", "", 1 } },

  { { TILEM_KEY_2ND, "2ND", "", "" },
    { TILEM_KEY_MODE, "MODE", "QUIT", "" },
    { TILEM_KEY_DEL, "DEL", "INS", "" },
    { TILEM_KEY_LEFT, "<", "", "" },
    { TILEM_KEY_RIGHT, ">", "", "" } },

  { { TILEM_KEY_ALPHA, "ALPHA", "A-LOCK", "" },
    { TILEM_KEY_GRAPHVAR, "X,T,θ,n", "LINK", "" },
    { TILEM_KEY_STAT, "STAT", "LIST", "" },
    { 0, "", "", "", 0.5F },
    { TILEM_KEY_DOWN, "V", "", "", 1 } },

  { { TILEM_KEY_MATH, "MATH", "TEST", "A" },
    { TILEM_KEY_MATRIX, "APPS", "ANGLE", "B" },
    { TILEM_KEY_PRGM, "PRGM", "DRAW", "C" },
    { TILEM_KEY_VARS, "VARS", "DISTR", "" },
    { TILEM_KEY_CLEAR, "CLEAR", "", "" } },

  { { TILEM_KEY_RECIP, "x^-1", "MATRIX", "D" },
    { TILEM_KEY_SIN, "SIN", "SIN^-1", "E" },
    { TILEM_KEY_COS, "COS", "COS^-1", "F" },
    { TILEM_KEY_TAN, "TAN", "TAN^-1", "G" },
    { TILEM_KEY_POWER, "^", "π", "H" } },

  { { TILEM_KEY_SQUARE, "x²", "√", "I" },
    { TILEM_KEY_COMMA, ",", "EE", "J" },
    { TILEM_KEY_LPAREN, "(", "{", "K" },
    { TILEM_KEY_RPAREN, ")", "}", "L" },
    { TILEM_KEY_DIV, "/", "e", "M" } },

  { { TILEM_KEY_LOG, "LOG", "10^x", "N" },
    { TILEM_KEY_7, "7", "u", "O" },
    { TILEM_KEY_8, "8", "v", "P" },
    { TILEM_KEY_9, "9", "w", "Q" },
    { TILEM_KEY_MUL, "*", "[", "R" } },

  { { TILEM_KEY_LN, "LN", "e^x", "S" },
    { TILEM_KEY_4, "4", "L4", "T" },
    { TILEM_KEY_5, "5", "L5", "U" },
    { TILEM_KEY_6, "6", "L6", "V" },
    { TILEM_KEY_SUB, "-", "]", "W" } },

  { { TILEM_KEY_STORE, "STO>", "RCL", "X" },
    { TILEM_KEY_1, "1", "L1", "Y" },
    { TILEM_KEY_2, "2", "L2", "Z" },
    { TILEM_KEY_3, "3", "L3", "θ" },
    { TILEM_KEY_ADD, "+", "MEM", "\"" } },

  { { TILEM_KEY_ON, "ON", "OFF", "" },
    { TILEM_KEY_0, "0", "CATALOG", "_" },
    { TILEM_KEY_DECPNT, ".", "i", ":" },
    { TILEM_KEY_CHS, "(-)", "ANS", "?" },
    { TILEM_KEY_ENTER, "ENTER", "ENTRY", "SOLVE" } },

};

} // namespace

Keypad::Keypad(TilemCalc* calc) : calc(calc), numRows(keymap.size()) {
  maxRowSize = std::max_element(keymap.begin(),
                                keymap.end(),
                                [](const auto& a, const auto& b) {
                                  return a.size() < b.size();
                                })
                 ->size();
}

std::unique_ptr<rmlib::RenderObject>
Keypad::createRenderObject() const {
  return std::make_unique<KeypadRenderObject>(*this);
}

rmlib::Size
KeypadRenderObject::doLayout(const Constraints& constraints) {
  keyWidth = int(constraints.max.width / widget->maxRowSize);
  keyHeight = int(keyWidth / key_aspect);

  const auto width = std::clamp(keyWidth * int(widget->maxRowSize),
                                constraints.min.width,
                                constraints.max.width);
  const auto height = std::clamp(keyHeight * int(widget->numRows),
                                 constraints.min.height,
                                 constraints.max.height);
  padding = int(constraints.max.width - (keyWidth * widget->maxRowSize));

  return { width, height };
}

void
KeypadRenderObject::drawKey(rmlib::Canvas& canvas,
                            rmlib::Point pos,
                            const Key& key,
                            int keyWidth) const {
  const auto frontLabelHeight = key.shift.empty() && key.alpha.empty()
                                  ? keyHeight
                                  : int(front_label_factor * keyHeight);
  const auto upperLabelHeight = keyHeight - frontLabelHeight;

  // Draw front label.
  {
    const auto fontSize = std::min(
      frontLabelHeight, int(key_aspect * keyWidth / double(key.front.size())));
    const auto fontSizes = Canvas::getTextSize(key.front, fontSize);

    const auto xOffset = (keyWidth - fontSizes.width) / 2;
    const auto yOffset =
      upperLabelHeight + ((frontLabelHeight - fontSizes.height) / 2);
    const auto position = pos + Point{ xOffset, yOffset };

    canvas.drawText(key.front, position, fontSize);
  }

  // Draw alpha and 2nd label.
  [&] {
    const auto upperLength = key.alpha.size() + key.shift.size();
    if (upperLength == 0) {
      return;
    }

    const auto fontSize =
      std::min(upperLabelHeight, int(1.6 * keyWidth / double(upperLength)));

    auto testStr = std::string(key.shift);
    if (!key.alpha.empty()) {
      testStr += " " + std::string(key.alpha);
    }

    const auto fontSizes = Canvas::getTextSize(testStr, fontSize);

    const auto xOffset = (keyWidth - fontSizes.width) / 2;
    const auto yOffset = (upperLabelHeight - fontSizes.height) / 2;

    const auto position = pos + Point{ xOffset, yOffset };

    canvas.drawText(key.shift, position, fontSize, shift_color);

    if (!key.alpha.empty()) {
      const auto spacing =
        Canvas::getTextSize(std::string(key.shift) + " ", fontSize);
      const auto positonA = pos + Point{ xOffset + spacing.width, yOffset };
      canvas.drawText(key.alpha, positonA, fontSize, alpha_color);
    }
  }();

  canvas.drawRectangle(pos, pos + Point{ keyWidth - 1, keyHeight - 1 }, black);
}

rmlib::UpdateRegion
KeypadRenderObject::doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) {
  keyLocations.clear();
  canvas.set(rect, white);

  int y = rect.topLeft.y;
  for (const auto& row : keymap) {

    int padding = this->padding;
    int x = rect.topLeft.x;
    for (const auto& key : row) {
      auto keyW = int(float(keyWidth) * key.width);
      if (padding > 0) {
        keyW += 1;
        padding -= 1;
      }

      if (key.scancode != 0) {
        keyLocations.emplace_back(
          Rect{ { x, y }, { x + keyW - 1, y + keyHeight - 1 } }, &key);
        drawKey(canvas, { x, y }, key, keyW);
      }

      x += keyW;
    }

    y += keyHeight;
  }

  return { rect };
}

void
KeypadRenderObject::handleInput(const Event& ev) {
  if (widget->calc == nullptr) {
    return;
  }

  std::visit(
    [this](const auto& ev) {
      if constexpr (is_pointer_event<decltype(ev)>) {
        if (ev.isMove()) {
          return;
        }

        if (ev.isUp()) {
          auto it = keyPointers.find(ev.id);
          if (it == keyPointers.end()) {
            return;
          }

          std::cout << "Releasing: " << it->second->scancode << "\n";
          tilem_keypad_release_key(widget->calc, it->second->scancode);
          keyPointers.erase(it);
          return;
        }

        if (ev.isDown()) {
          for (const auto& [rect, keyPtr] : keyLocations) {
            if (rect.contains(ev.location)) {
              std::cout << "Pressing: " << keyPtr->scancode << "\n";
              tilem_keypad_press_key(widget->calc, keyPtr->scancode);
              keyPointers.emplace(ev.id, keyPtr);
              break;
            }
          }
        }
      }
    },
    ev);
}
} // namespace tilem
