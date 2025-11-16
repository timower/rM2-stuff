#include "screen.h"
#include "conf.h"

using namespace rmlib;

namespace {
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
  for (int i = 0; i < newWidget.term->lines; i++) {
    if (newWidget.term->line_dirty[i]) {
      markNeedsDraw(/* full */ false);
    }
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
    term_resize(widget->term, size.height, size.width, /* report */ true);
  } else {
    term_resize(widget->term, size.width, size.height, /* report */ true);
  }
  return size;
}

bool
ScreenRenderObject::shouldRefresh() const {
  return isFullDraw() || widget->term->shouldClear ||
         (widget->autoRefresh > 0 && numUpdates > widget->autoRefresh);
}

rmlib::UpdateRegion
ScreenRenderObject::doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) {
  auto& term = *widget->term;

  if ((term.mode & MODE_CURSOR) != 0U) {
    term.line_dirty[term.cursor.y] = true;
  }

  Rect currentRect;

  const auto maybeDraw = [&](bool last = false) {
    if (currentRect.empty() || shouldRefresh()) {
      return;
    }

    bool useA2 = widget->isLandscape
                   ? term.lines * CELL_HEIGHT <= currentRect.width()
                   : term.lines * CELL_HEIGHT <= currentRect.height();
    fb->doUpdate(currentRect,
                 useA2 ? fb::Waveform::A2 : fb::Waveform::DU,
                 useA2 ? fb::UpdateFlags::None : fb::UpdateFlags::Priority);

    currentRect = {};
    numUpdates++;
  };

  for (int line = 0; line < term.lines; line++) {
    if (isFullDraw() || term.line_dirty[line]) {
      currentRect |= drawLine(canvas, rect, term, line);
    } else {
      maybeDraw();
    }
  }
  maybeDraw(/* last */ true);

  if (shouldRefresh()) {
    term.shouldClear = false;
    numUpdates = 0;
    return { rect, fb::Waveform::GC16, fb::UpdateFlags::FullRefresh };
  }

  return {};
}

rmlib::Rect
ScreenRenderObject::drawLine(rmlib::Canvas& canvas,
                             rmlib::Rect rect,
                             terminal_t& term,
                             int line) const {

  const bool isLandscape = widget->isLandscape;

  // x in landscape, y in portrait.
  int zStart =
    isLandscape
      ? term.height - (term.marginTop + line * CELL_HEIGHT) + rect.topLeft.x
      : term.marginTop + line * CELL_HEIGHT + rect.topLeft.y;

  for (int col = 0; col < term.cols; col++) {
    int marginLeft = term.marginLeft + col * CELL_WIDTH +
                     (isLandscape ? rect.topLeft.y : rect.topLeft.x);

    auto& cell = term.cells[line][col];

    if (cell.has_pixmap) {
      // TODO
      continue;
    }

    auto colorPair = cell.color_pair; // copy

    /* check wide character or not */
    int glyphWidth = (cell.width == HALF) ? CELL_WIDTH : CELL_WIDTH * 2;
    int bdfPadding =
      myCeil(glyphWidth, BITS_PER_BYTE) * BITS_PER_BYTE - glyphWidth;
    if (cell.width == WIDE) {
      bdfPadding += CELL_WIDTH;
    }

    /* check cursor position */
    if ((((term.mode & MODE_CURSOR) != 0U) && line == term.cursor.y) &&
        (col == term.cursor.x ||
         (cell.width == WIDE && (col + 1) == term.cursor.x) ||
         (cell.width == NEXT_TO_WIDE && (col - 1) == term.cursor.x))) {
      colorPair.fg = DEFAULT_BG;
      colorPair.bg = (/*!vt_active &&*/ BACKGROUND_DRAW) != 0U
                       ? PASSIVE_CURSOR_COLOR
                       : ACTIVE_CURSOR_COLOR;
    }

    // lookup pixels
    const auto bgBright = color2brightness(color_list[colorPair.bg]);
    const auto fgBright = color2brightness(color_list[colorPair.fg]);

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
      if ((h == (CELL_HEIGHT - 1)) &&
          ((cell.attribute & attr_mask[ATTR_UNDERLINE]) != 0)) {
        std::swap(bgGray, fgGray);
      }

      for (int w = 0; w < CELL_WIDTH; w++) {
        const auto pos = isLandscape ? Point{ zStart - h, marginLeft + w }
                                     : Point{ marginLeft + w, zStart + h };

        /* set fg or bg */
        const auto* glyph = (cell.attribute & ATTR_BOLD) != 0
                              ? cell.glyph.boldp
                              : cell.glyph.regularp;

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

  term.line_dirty[line] =
    ((term.mode & MODE_CURSOR) != 0u) && term.cursor.y == line;

  return isLandscape
           ? Rect{ { zStart - CELL_HEIGHT, 0 }, { zStart, rect.height() - 1 } }
           : Rect{ { 0, zStart },
                   { rect.width() - 1, zStart + CELL_HEIGHT - 1 } };
}

template<typename Ev>
void
ScreenRenderObject::handleTouchEvent(const Ev& ev) {
  if ((widget->term->mode & ALL_MOUSE_MODES) == 0) {
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

  const auto scaledLoc = ev.location - getRect().topLeft;
  const auto rotatedLoc =
    widget->isLandscape ? Point{ scaledLoc.y, getRect().width() - scaledLoc.x }
                        : scaledLoc;

  std::array<char, 6> buf{};
  initMouseBuf(buf, rotatedLoc);

  // Mouse down on first finger if mouse is not down.
  if (ev.isDown() && mouseSlot == -1 /*&& lastFingers == 0*/) {
    mouseSlot = slot;

    // Send mouse down code
    buf[3] += 0; // mouse button 1
    write(widget->term->fd, buf.data(), buf.size());

  } else if ((ev.isUp() && slot == mouseSlot) /*||
             (kb.mouseSlot != -1 && kb.gestureCtrlr.getCurrentFingers() > 1)*/) {

    // Mouse up after finger lift or second finger down (for scrolling).
    mouseSlot = -1;

    // Send mouse up code
    buf[3] += 3; // mouse release
    write(widget->term->fd, buf.data(), buf.size());
  } else if (mouseSlot == slot && lastMousePos != rotatedLoc &&
             (widget->term->mode & MODE_MOUSE_MOVE) != 0) {
    buf[3] += 32; // mouse move
    write(widget->term->fd, buf.data(), buf.size());
  }
  lastMousePos = rotatedLoc;
}

void
ScreenRenderObject::handleInput(const rmlib::input::Event& ev) {
  std::visit(
    [this](const auto& ev) {
      if constexpr (rmlib::input::is_pointer_event<
                      std::decay_t<decltype(ev)>>) {

        if (getRect().contains(ev.location)) {
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
