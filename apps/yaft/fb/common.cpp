#include "common.h"

#include "conf.h"
#include "util.h"

#include <iostream>

namespace {

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

inline void
draw_sixel(rmlib::fb::FrameBuffer& fb,
           struct terminal_t* term,
           bool isLandscape,
           int y_start,
           int margin_left,
           int col,
           uint8_t* pixmap) {
  int h, w, src_offset, dst_offset;
  uint32_t pixel, color = 0;

  for (h = 0; h < CELL_HEIGHT; h++) {
    for (w = 0; w < CELL_WIDTH; w++) {
      src_offset = BYTES_PER_PIXEL * (h * CELL_WIDTH + w);
      memcpy(&color, pixmap + src_offset, BYTES_PER_PIXEL);

      if (isLandscape) {
        dst_offset = (margin_left + w) * fb.canvas.lineSize() +
                     (y_start - h) * fb.canvas.components();
      } else {
        dst_offset = (y_start + h) * fb.canvas.lineSize() +
                     (margin_left + w) * fb.canvas.components();
      }
      auto grayMode = brightness2gray(color2brightness(color));

      switch (grayMode) {
        case White:
          pixel = 0xFFFF;
          break;
        case Dither:
          pixel = (h % 2) == (w % 2) ? 0x0 : 0xFFFF;
          break;
        case Black:
          pixel = 0;
          break;
      }
      memcpy(
        fb.canvas.getMemory() + dst_offset, &pixel, fb.canvas.components());
    }
  }
}

static int update_count = 0;

inline rmlib::Rect
draw_line(rmlib::fb::FrameBuffer& fb,
          struct terminal_t* term,
          bool isLandscape,
          int line) {
  int pos, bdf_padding, glyph_width, margin_left, y_start;
  int col, w, h;
  uint32_t pixel;
  struct color_pair_t color_pair;
  struct cell_t* cellp;

  if (isLandscape) {
    y_start = term->height - (term->marginTop + line * CELL_HEIGHT);
  } else {
    y_start = term->marginTop + line * CELL_HEIGHT;
  }

  for (col = 0; col < term->cols; col++) {
    margin_left = term->marginLeft + col * CELL_WIDTH;

    /* target cell */
    cellp = &term->cells[line][col];

    /* draw sixel pixmap */
    if (cellp->has_pixmap) {
      draw_sixel(
        fb, term, isLandscape, y_start, margin_left, col, cellp->pixmap);
      continue;
    }

    /* copy current color_pair (maybe changed) */
    color_pair = cellp->color_pair;

    /* check wide character or not */
    glyph_width = (cellp->width == HALF) ? CELL_WIDTH : CELL_WIDTH * 2;
    bdf_padding =
      my_ceil(glyph_width, BITS_PER_BYTE) * BITS_PER_BYTE - glyph_width;
    if (cellp->width == WIDE)
      bdf_padding += CELL_WIDTH;

    /* check cursor position */
    if ((term->mode & MODE_CURSOR && line == term->cursor.y) &&
        (col == term->cursor.x ||
         (cellp->width == WIDE && (col + 1) == term->cursor.x) ||
         (cellp->width == NEXT_TO_WIDE && (col - 1) == term->cursor.x))) {
      color_pair.fg = DEFAULT_BG;
      color_pair.bg = (!vt_active && BACKGROUND_DRAW) ? PASSIVE_CURSOR_COLOR
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

    for (h = 0; h < CELL_HEIGHT; h++) {
      /* if UNDERLINE attribute on, swap bg/fg */
      if ((h == (CELL_HEIGHT - 1)) &&
          (cellp->attribute & attr_mask[ATTR_UNDERLINE])) {
        std::swap(bg_gray, fg_gray);
      }

      for (w = 0; w < CELL_WIDTH; w++) {
        if (isLandscape) {
          pos = (margin_left + w) * fb.canvas.lineSize() +
                (y_start - h) * fb.canvas.components();
        } else {
          pos = (margin_left + w) * fb.canvas.components() +
                (y_start + h) * fb.canvas.lineSize();
        }

        /* set fg or bg */
        const auto* glyph = (cellp->attribute & ATTR_BOLD) != 0
                              ? cellp->glyph.boldp
                              : cellp->glyph.regularp;

        const auto grayMode =
          (glyph->bitmap[h] & (0x01 << (bdf_padding + CELL_WIDTH - 1 - w)))
            ? fg_gray
            : bg_gray;

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
        memcpy(fb.canvas.getMemory() + pos, &pixel, fb.canvas.components());
      }
    }
  }

  term->line_dirty[line] =
    ((term->mode & MODE_CURSOR) && term->cursor.y == line) ? true : false;

  return isLandscape ? rmlib::Rect{ { y_start - CELL_HEIGHT, 0 },
                                    { y_start, term->width - 1 } }
                     : rmlib::Rect{ { 0, y_start },
                                    { fb.canvas.width() - 1,
                                      y_start + CELL_HEIGHT - 1 } };
}

} // namespace

void
refresh(rmlib::fb::FrameBuffer& fb, struct terminal_t* term, bool isLandscape) {
  if (term->mode & MODE_CURSOR)
    term->line_dirty[term->cursor.y] = true;

  rmlib::Rect currentRegion;

  const auto maybeDraw = [&] {
    if (currentRegion.empty() || term->shouldClear) {
      return;
    }
    fb.doUpdate(
      currentRegion, rmlib::fb::Waveform::DU, rmlib::fb::UpdateFlags::None);
    currentRegion = {};
    update_count++;
  };

  for (int line = 0; line < term->lines; line++) {
    if (term->line_dirty[line]) {
      currentRegion |= draw_line(fb, term, isLandscape, line);
    } else {
      maybeDraw();
    }
  }
  maybeDraw();

  if (term->shouldClear || update_count > 1024) {
    std::cout << "FULL UPDATE: " << update_count << std::endl;
    fb.doUpdate(fb.canvas.rect(),
                rmlib::fb::Waveform::GC16,
                rmlib::fb::UpdateFlags::FullRefresh);

    term->shouldClear = false;
    update_count = 0;
  }
}
