#include "screen.h"
#include "color.h"
#include "conf.h"

using namespace rmlib;

namespace {
constexpr int BITS_PER_BYTE = 8;

// TODO: move
// \{
inline int
myCeil(int val, int div) {
  if (div == 0) {
    return 0;
  }
  return (val + div - 1) / div;
}

inline uint16_t
color2brightness(uint32_t color) {
  int r = 0xFF & (color >> (BITS_PER_RGB * 2));
  int g = 0xFF & (color >> BITS_PER_RGB);
  int b = 0xFF & (color >> 0);

  return (54 * r + 182 * g + 19 * b) / 255;
}

enum GrayMode {
  Black = 0,
  Dither = 1,
  White = 2,
};

GrayMode
brightness2gray(uint16_t brightness) {
  if (brightness <= 85) {
    return Black;
  }
  if (brightness < 170) {
    return Dither;
  }
  return White;
}

void
initMouseBuf(std::array<char, 6>& buf, Point loc) {
  constexpr char esc_char = '\x1b';

  loc.x /= CELL_WIDTH;
  loc.y /= CELL_HEIGHT;

  char cx = 33 + loc.x;
  char cy = 33 + loc.y;
  buf = { esc_char, '[', 'M', 32, cx, cy };
}
// \}
} // namespace

std::unique_ptr<rmlib::RenderObject>
Screen::createRenderObject() const {
  return std::make_unique<ScreenRenderObject>(*this);
}

void
ScreenRenderObject::update(const Screen& newWidget) {
  // TODO: correct location?
  auto& term = *newWidget.term;
  const int cursorRow = term.cursorVisible ? term.cursorY : -1;
  if (term.hasPendingDirty() || cursorRow != lastCursorRow) {
    markNeedsDraw(/* full */ false);
  }

  if (newWidget.isLandscape != widget->isLandscape) {
    markNeedsLayout();
    markNeedsDraw(/* full */ true);
  }

  widget = &newWidget;
}

rmlib::Size
ScreenRenderObject::doLayout(const rmlib::Constraints& constraints) {
  const auto size = constraints.isBounded() ? constraints.max : constraints.min;
  assert(size.width != 0 && size.height != 0);

  if (widget->isLandscape) {
    widget->term->resize(size.height, size.width, /* report */ true);
  } else {
    widget->term->resize(size.width, size.height, /* report */ true);
  }
  return size;
}

bool
ScreenRenderObject::shouldRefresh() const {
  return isFullDraw() || widget->term->shouldClear ||
         (widget->autoRefresh > 0 && numUpdates > widget->autoRefresh);
}

rmlib::UpdateRegion
ScreenRenderObject::doDraw(rmlib::Canvas& canvas) {
  auto& term = *widget->term;
  term.beginDraw();
  const int cursorRow = term.cursorVisible ? term.cursorY : -1;

  Rect currentRect;

  const auto maybeDraw = [&](bool last = false) {
    if (currentRect.empty() || shouldRefresh()) {
      return;
    }

    bool useA2 = widget->isLandscape
                   ? term.lines * CELL_HEIGHT <= currentRect.width()
                   : term.lines * CELL_HEIGHT <= currentRect.height();
    fb->doUpdate(canvas.subCanvas(currentRect),
                 useA2 ? fb::Waveform::A2 : fb::Waveform::DU,
                 useA2 ? fb::UpdateFlags::None : fb::UpdateFlags::FastDraw);

    currentRect = {};
    numUpdates++;
  };

  for (int line = 0; line < term.lines; line++) {
    term.nextRow();
    // Redraw both the cursor's current row and the row it just left --
    // ghostty never marks either dirty on its own, since it has no concept
    // of our drawn cursor overlay.
    const bool cursorHere = line == cursorRow || line == lastCursorRow;
    if (isFullDraw() || term.isRowDirty() || cursorHere) {
      currentRect |= drawLine(canvas, term, line);
    } else {
      maybeDraw();
    }
  }
  maybeDraw(/* last */ true);
  lastCursorRow = cursorRow;

  if (shouldRefresh()) {
    term.consumeShouldClear();
    numUpdates = 0;
    return { canvas.rect(), fb::Waveform::GC16, fb::UpdateFlags::Sync };
  }

  return {};
}

rmlib::Rect
ScreenRenderObject::drawLine(rmlib::Canvas& canvas,
                             Terminal& term,
                             int line) const {

  const bool isLandscape = widget->isLandscape;

  // x in landscape, y in portrait.
  int zStart = isLandscape ? term.height - (term.marginTop + line * CELL_HEIGHT)
                           : term.marginTop + line * CELL_HEIGHT;

  for (int col = 0; col < term.cols; col++) {
    int marginLeft = term.marginLeft + col * CELL_WIDTH;

    auto cell = term.cellAt(col);

    if (cell.hasPixmap) {
      // TODO
      continue;
    }

    uint32_t fg = cell.fg;
    uint32_t bg = cell.bg;

    /* check wide character or not */
    int glyphWidth =
      (cell.width == CellWidth::Half) ? CELL_WIDTH : CELL_WIDTH * 2;
    int bdfPadding =
      myCeil(glyphWidth, BITS_PER_BYTE) * BITS_PER_BYTE - glyphWidth;
    if (cell.width == CellWidth::Wide) {
      bdfPadding += CELL_WIDTH;
    }

    /* check cursor position */
    if ((term.cursorVisible && line == term.cursorY) &&
        (col == term.cursorX ||
         (cell.width == CellWidth::Wide && (col + 1) == term.cursorX) ||
         (cell.width == CellWidth::NextToWide && (col - 1) == term.cursorX))) {
      fg = color_list[DEFAULT_BG];
      bg = (/*!vt_active &&*/ BACKGROUND_DRAW) != 0U
             ? color_list[PASSIVE_CURSOR_COLOR]
             : color_list[ACTIVE_CURSOR_COLOR];
    }

    // lookup pixels
    const auto bgBright = color2brightness(bg);
    const auto fgBright = color2brightness(fg);

    auto bgGray = brightness2gray(bgBright);
    auto fgGray = brightness2gray(fgBright);

    // Don't draw same color
    if (fgGray == bgGray) {
      if (fgBright < bgBright) {
        if (fgGray != 0) {
          fgGray = static_cast<GrayMode>(fgGray - 1);
        } else {
          bgGray = static_cast<GrayMode>(bgGray + 1);
        }
      } else {
        if (bgGray != 0) {
          bgGray = static_cast<GrayMode>(bgGray - 1);
        } else {
          fgGray = static_cast<GrayMode>(fgGray + 1);
        }
      }
    }

    for (int h = 0; h < CELL_HEIGHT; h++) {
      /* if UNDERLINE attribute on, swap bg/fg */
      if ((h == (CELL_HEIGHT - 1)) && cell.underline) {
        std::swap(bgGray, fgGray);
      }

      for (int w = 0; w < CELL_WIDTH; w++) {
        const auto pos = isLandscape ? Point{ zStart - h, marginLeft + w }
                                     : Point{ marginLeft + w, zStart + h };

        /* set fg or bg */
        const auto* glyph = cell.bold ? cell.boldGlyph : cell.glyph;

        const auto grayMode =
          (glyph->bitmap[h] & (0x01 << (bdfPadding + CELL_WIDTH - 1 - w))) != 0U
            ? fgGray
            : bgGray;

        int pixel = 0;
        switch (grayMode) {
          case White:
            pixel = 0; // 0xFFFF;
            break;
          case Dither:
            pixel = (h % 2) == (w % 2) ? 0x0 : 0xFFFF;
            break;
          case Black:
            pixel = 0xFFFF; // 0;
            break;
        }

        // We only care about rgb555, as we assume that format above.
        // So do a direct assign instead of memcpy.
        canvas.setPixel(pos, pixel);
      }
    }
  }

  term.clearRowDirty();

  const auto size = this->getSize();
  return isLandscape
           ? Rect{ { zStart - CELL_HEIGHT, 0 }, { zStart, size.height - 1 } }
           : Rect{ { 0, zStart },
                   { size.width - 1, zStart + CELL_HEIGHT - 1 } };
}

template<typename Ev>
void
ScreenRenderObject::handleTouchEvent(const Ev& ev) {
  if (!widget->term->mouseTrackingEnabled()) {
    return;
  }

  // TODO: two finger scrolling?
  // const auto lastFingers = kb.gestureCtrlr.getCurrentFingers();
  // if constexpr (std::is_same_v<Event, TouchEvent>) {
  //   const auto [gestures, _] = kb.gestureCtrlr.handleEvents({ ev });
  //   for (const auto& gesture : gestures) {
  //     if (std::holds_alternative<SwipeGesture>(gesture)) {
  //       handleGesture(kb, std::get<SwipeGesture>(gesture));
  //     }
  //   }
  // }

  const auto slot = [&ev]() {
    if constexpr (std::is_same_v<Ev, input::PenEvent>) {
      (void)ev;
      return 0x1000;
    } else {
      return ev.slot;
    }
  }();

  const auto scaledLoc = ev.location;
  const auto rotatedLoc =
    widget->isLandscape ? Point{ scaledLoc.y, getSize().width - scaledLoc.x }
                        : scaledLoc;

  std::array<char, 6> buf{};
  initMouseBuf(buf, rotatedLoc);

  // Mouse down on first finger if mouse is not down.
  if (ev.isDown() && mouseSlot == -1 /*&& lastFingers == 0*/) {
    mouseSlot = slot;

    // Send mouse down code
    buf[3] += 0; // mouse button 1
    widget->term->write(reinterpret_cast<const uint8_t*>(buf.data()),
                        buf.size());

  } else if ((ev.isUp() && slot == mouseSlot) /*||
             (kb.mouseSlot != -1 && kb.gestureCtrlr.getCurrentFingers() > 1)*/) {

    // Mouse up after finger lift or second finger down (for scrolling).
    mouseSlot = -1;

    // Send mouse up code
    buf[3] += 3; // mouse release
    widget->term->write(reinterpret_cast<const uint8_t*>(buf.data()),
                        buf.size());
  } else if (mouseSlot == slot && lastMousePos != rotatedLoc &&
             widget->term->mouseMoveMode()) {
    buf[3] += 32; // mouse move
    widget->term->write(reinterpret_cast<const uint8_t*>(buf.data()),
                        buf.size());
  }
  lastMousePos = rotatedLoc;
}

void
ScreenRenderObject::doHandleInput(const rmlib::input::Event& ev) {
  std::visit(
    [this](const auto& ev) {
      if constexpr (rmlib::input::is_pointer_event<
                      std::decay_t<decltype(ev)>>) {

        if (getLocalRect().contains(ev.location)) {
          handleTouchEvent(ev);
        }
      }
    },
    ev);
}

void
ScreenRenderObject::doRebuild(AppContext& ctx,
                              const BuildContext& /*buildContext*/) {
  this->fb = &ctx.getFramebuffer();
}
