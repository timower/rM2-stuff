#include "screen.h"
#include "conf.h"

using namespace rmlib;

namespace {
// TODO: move
// \{
static inline int
my_ceil(int val, int div) {
  if (div == 0)
    return 0;
  else
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

  widget = &newWidget;
}

rmlib::Size
ScreenRenderObject::doLayout(const rmlib::Constraints& constraints) {
  const auto size = constraints.isBounded() ? constraints.max : constraints.min;
  assert(size.width != 0 && size.height != 0);

  term_resize(widget->term, size.width, size.height, /* report */ true);
  return size;
}

rmlib::UpdateRegion
ScreenRenderObject::doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) {
  auto& term = *widget->term;

  if (term.mode & MODE_CURSOR) {
    term.line_dirty[term.cursor.y] = true;
  }

  UpdateRegion result;

  for (int line = 0; line < term.lines; line++) {
    if (!isPartialDraw() || term.line_dirty[line]) {
      result |= drawLine(canvas, rect, term, line);
    }
  }

  // TODO: update count tracker?
  if (term.shouldClear) {
    result |=
      UpdateRegion(rect, fb::Waveform::GC16, fb::UpdateFlags::FullRefresh);
    term.shouldClear = false;
  }

  return result;
}

rmlib::UpdateRegion
ScreenRenderObject::drawLine(rmlib::Canvas& canvas,
                             rmlib::Rect rect,
                             terminal_t& term,
                             int line) const {
  int y_start = term.marginTop + line * CELL_HEIGHT + rect.topLeft.y;

  for (int col = 0; col < term.cols; col++) {
    int margin_left = (rect.width() - term.cols * CELL_WIDTH) / 2 +
                      col * CELL_WIDTH + rect.topLeft.x;

    auto& cell = term.cells[line][col];

    if (cell.has_pixmap) {
      // TODO
      continue;
    }

    auto color_pair = cell.color_pair; // copy

    /* check wide character or not */
    int glyph_width = (cell.width == HALF) ? CELL_WIDTH : CELL_WIDTH * 2;
    int bdf_padding =
      my_ceil(glyph_width, BITS_PER_BYTE) * BITS_PER_BYTE - glyph_width;
    if (cell.width == WIDE)
      bdf_padding += CELL_WIDTH;

    /* check cursor position */
    if ((term.mode & MODE_CURSOR && line == term.cursor.y) &&
        (col == term.cursor.x ||
         (cell.width == WIDE && (col + 1) == term.cursor.x) ||
         (cell.width == NEXT_TO_WIDE && (col - 1) == term.cursor.x))) {
      color_pair.fg = DEFAULT_BG;
      color_pair.bg = (/*!vt_active &&*/ BACKGROUND_DRAW) ? PASSIVE_CURSOR_COLOR
                                                          : ACTIVE_CURSOR_COLOR;
    }

    // lookup pixels
    const auto bg_bright = color2brightness(color_list[color_pair.bg]);
    const auto fg_bright = color2brightness(color_list[color_pair.fg]);

    auto bg_gray = brightness2gray(bg_bright);
    auto fg_gray = brightness2gray(fg_bright);

    // Don't draw same color
    if (fg_gray == bg_gray) {
      if (fg_bright < bg_bright) {
        if (fg_gray != 0) {
          fg_gray = static_cast<GrayMode>(fg_gray - 1);
        } else {
          bg_gray = static_cast<GrayMode>(bg_gray + 1);
        }
      } else {
        if (bg_gray != 0) {
          bg_gray = static_cast<GrayMode>(bg_gray - 1);
        } else {
          fg_gray = static_cast<GrayMode>(fg_gray + 1);
        }
      }
    }

    for (int h = 0; h < CELL_HEIGHT; h++) {
      /* if UNDERLINE attribute on, swap bg/fg */
      if ((h == (CELL_HEIGHT - 1)) &&
          (cell.attribute & attr_mask[ATTR_UNDERLINE])) {
        std::swap(bg_gray, fg_gray);
      }

      for (int w = 0; w < CELL_WIDTH; w++) {
        int pos = (margin_left + w) * canvas.components() +
                  (y_start + h) * canvas.lineSize();

        /* set fg or bg */
        const auto* glyph = (cell.attribute & ATTR_BOLD) != 0
                              ? cell.glyph.boldp
                              : cell.glyph.regularp;

        const auto grayMode =
          (glyph->bitmap[h] & (0x01 << (bdf_padding + CELL_WIDTH - 1 - w)))
            ? fg_gray
            : bg_gray;

        int pixel;
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

        /* update copy buffer only */
        memcpy(canvas.getMemory() + pos, &pixel, canvas.components());
      }
    }
  }

  term.line_dirty[line] =
    ((term.mode & MODE_CURSOR) && term.cursor.y == line) ? true : false;

  return UpdateRegion(
    { { 0, y_start }, { rect.width() - 1, y_start + CELL_HEIGHT - 1 } },
    rmlib::fb::Waveform::DU,
    rmlib::fb::UpdateFlags::None);
}
