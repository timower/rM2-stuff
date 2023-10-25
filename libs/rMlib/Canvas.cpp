#include "Canvas.h"

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_truetype.h"

#include <array>
#include <codecvt>
#include <iostream>
#include <locale>
#include <vector>

#include "thick.cpp"

namespace rmlib {

namespace {
constexpr uint8_t
blend(uint8_t factor, uint8_t fg, uint8_t bg) {
  int val = bg + (int(factor) * (int(fg) - int(bg))) / 0xff;
  return uint8_t(val);
}

constexpr uint16_t
toRGB565(uint8_t grey) {
  return (grey >> 3) | ((grey >> 2) << 5) | ((grey >> 3) << 11);
}

constexpr uint8_t
fromRGB565(uint16_t rgb) {
  // uint8_t r = (rgb & 0x1f) << 3;
  uint8_t g = ((rgb >> 5) & 0x3f) << 2;
  // uint8_t b = ((rgb >> 11) & 0x1f) << 3;

  // Only use g for now, as it has the most bit depth.
  return g;
};

#ifdef EMULATE
#include "noto-sans-mono.h"
#else
constexpr auto font_path = "/usr/share/fonts/ttf/noto/NotoMono-Regular.ttf";
#endif

stbtt_fontinfo*
getFont() {
  static auto* font = [] {
    static stbtt_fontinfo font;
#ifndef EMULATE
    // TODO: unistdpp
    static std::array<uint8_t, 24 << 20> fontBuffer = { 0 };
    auto* fp = fopen(font_path, "r"); // NOLINT
    if (fp == nullptr) {
      std::cerr << "Error opening font\n";
    }
    auto len = fread(fontBuffer.data(), 1, fontBuffer.size(), fp);
    if (len == static_cast<decltype(len)>(-1)) {
      std::cerr << "Error reading font\n";
    }
    const auto* data = fontBuffer.data();
#else
    const auto* data = NotoSansMono_Regular_ttf;
#endif

    if (!stbtt_InitFont(&font, data, 0)) {
      std::cerr << "Error initializing font!\n";
      std::exit(EXIT_FAILURE);
    }

#ifndef EMULATE
    fclose(fp); // NOLINT
#endif
    return &font;
  }();

  return font;
}
} // namespace

Point
Canvas::getTextSize(std::string_view text, int size) {
  const auto* font = getFont();
  auto scale = stbtt_ScaleForPixelHeight(font, float(size));

  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);

  // Divide the line gap to above and below.
  float charStart = float(lineGap) * scale / 2;

  float baseLine = charStart + float(ascent) * scale;
  float charEnd = baseLine - float(descent) * scale; // descent is negative.
  float height = charEnd + charStart;

  auto utf32 =
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>{}.from_bytes(
      text.data());

  float xpos = 0;
  for (int ch = 0; utf32[ch] != 0; ch++) {
    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(font, utf32[ch], &advance, &lsb);

    xpos += float(advance) * scale;
    if (utf32[ch + 1] != 0) {
      xpos +=
        scale *
        float(stbtt_GetCodepointKernAdvance(font, utf32[ch], utf32[ch + 1]));
    }
  }

  return { static_cast<int>(ceilf(xpos)), static_cast<int>(ceilf(height)) };
}

void
Canvas::drawText(std::string_view text,
                 Point location,
                 int size,
                 int fg,
                 int bg,
                 std::optional<Rect> optClipRect) { // NOLINT
  const auto clipRect = optClipRect.has_value() ? *optClipRect : rect();

  const auto* font = getFont();
  auto scale = stbtt_ScaleForPixelHeight(font, float(size));

  int ascent = 0;
  int descent = 0;
  int lineGap = 0;
  stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);

  // Divide the line gap to above and below.
  float charStart = float(lineGap) * scale / 2;
  float baseLine = charStart + float(ascent) * scale;
  float yShfit = 0; // baseLine - floorf(baseLine);

  auto utf32 =
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>{}.from_bytes(
      text.data());

  std::vector<uint8_t> textBuffer;

  float xpos = 0;
  for (int ch = 0; utf32[ch] != 0; ch++) {
    float xShift = xpos - floorf(xpos);

    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(font, utf32[ch], &advance, &lsb);

    int x0 = 0;
    int x1 = 0;
    int y0 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBox(
      font, utf32[ch], scale, scale, &x0, &y0, &x1, &y1);

    int w = x1 - x0 + 1;
    int h = y1 - y0 + 1;
    int size = w * h;
    if (static_cast<unsigned>(size) > textBuffer.size()) {
      textBuffer.resize(size);
    }

    stbtt_MakeCodepointBitmapSubpixel(font,
                                      textBuffer.data(),
                                      /*  width */ w,
                                      /* height */ h,
                                      /* stride */ w,
                                      /* xscale */ scale,
                                      /* yscale */ scale,
                                      xShift,
                                      yShfit,
                                      utf32[ch]);

    const auto fg8 = fg & 0xff;
    const auto bg8 = bg & 0xff;

    // Draw the bitmap to canvas.
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        auto t = textBuffer[y * w + x];
        // int pixel = bg8 + (t * (fg8 - bg8)) / 0xff;
        auto pixel = blend(t, fg8, bg8);
        // auto pixel = (0xff - textBuffer[y * w + x]);
        uint16_t pixel565 = toRGB565(pixel);
        // (pixel >> 3) | ((pixel >> 2) << 5) | ((pixel >> 3) << 11);

        auto memY = location.y + static_cast<int>(baseLine) + y0 + y /*- 1*/;
        auto memX = location.x + static_cast<int>(xpos) + x0 + x;
        if (memX < clipRect.topLeft.x || clipRect.bottomRight.x < memX ||
            memY < clipRect.topLeft.y || clipRect.bottomRight.y < memY) {
          continue;
        }

        auto* targetPtr = getPtr<uint16_t>(memX, memY);
        *targetPtr = pixel565;
      }
    }

    // note that this stomps the old data, so where character boxes overlap
    // (e.g. 'lj') it's wrong because this API is really for baking character
    // bitmaps into textures. if you want to render a sequence of characters,
    // you really need to render each bitmap to a temp font_buffer, then "alpha
    // blend" that into the working font_buffer
    xpos += float(advance) * scale;
    if (utf32[ch + 1] != 0) {
      xpos +=
        scale *
        float(stbtt_GetCodepointKernAdvance(font, utf32[ch], utf32[ch + 1]));
    }
  }
}

void
Canvas::drawLine(Point start, Point end, int val, int thickness) {
  if (thickness != 1) {
    return draw_thick_line(
      *this, val, start.x, start.y, end.x, end.y, thickness);
  }

  int dx = abs(end.x - start.x);
  int sx = start.x < end.x ? 1 : -1;

  int dy = abs(end.y - start.y);
  int sy = start.y < end.y ? 1 : -1;

  int err = (dx > dy ? dx : -dy) / 2;

  for (;;) {
    setPixel(start, val);
    if (start == end) {
      break;
    }

    int e2 = err;
    if (e2 > -dx) {
      err -= dy;
      start.x += sx;
    }
    if (e2 < dy) {
      err += dx;
      start.y += sy;
    }
  }
}

void
Canvas::drawDisk(Point center, int radius, int val) {
  for (int dy = -radius; dy < radius; dy++) {
    for (int dx = -radius; dx < radius; dx++) {
      if (dx * dx + dy * dy < radius * radius) {
        setPixel(center + Point{ dx, dy }, val);
      }
    }
  }
}

constexpr uint16_t
greyAlphaToRGB565(uint8_t background, uint16_t pixel) {
  uint8_t grey = pixel & 0xff;
  uint8_t alpha = (pixel >> 8);

  auto blended = blend(alpha, grey, background);

  return toRGB565(blended);
}

std::optional<ImageCanvas>
ImageCanvas::loadRaw(const char* path) {
  int width;
  int height;
  int imgComponents;
  auto* mem = stbi_load(path, &width, &height, &imgComponents, 2);
  if (mem == nullptr) {
    return std::nullopt;
  }

  return ImageCanvas{ Canvas(mem, width, height, 2) };
}

std::optional<ImageCanvas>
ImageCanvas::load(const char* path, int background) {
  auto result = loadRaw(path);
  if (!result.has_value()) {
    return {};
  }

  result->canvas.transform([background](auto x, auto y, uint16_t pixel) {
    return greyAlphaToRGB565(background, pixel);
  });
  return result;
}

std::optional<ImageCanvas>
ImageCanvas::load(uint8_t* data, int size, int background) {
  int width;
  int height;
  int imgComponents;
  auto* mem =
    stbi_load_from_memory(data, size, &width, &height, &imgComponents, 2);
  if (mem == nullptr) {
    return std::nullopt;
  }

  Canvas result(mem, width, height, 2);
  result.transform([background](auto x, auto y, uint16_t pixel) {
    return greyAlphaToRGB565(background, pixel);
  });
  return ImageCanvas{ result };
}

void
ImageCanvas::release() {
  if (canvas.getMemory() != nullptr) {
    stbi_image_free(canvas.getMemory());
  }
  canvas = Canvas{};
}

MemoryCanvas::MemoryCanvas(const Canvas& other, Rect rect) {
  // NOLINTNEXTLINE
  memory = std::make_unique<uint8_t[]>(rect.width() * rect.height() *
                                       other.components());
  canvas =
    Canvas(memory.get(), rect.width(), rect.height(), other.components());
  copy(canvas, { 0, 0 }, other, rect);
}

MemoryCanvas::MemoryCanvas(int width, int height, int components) {
  memory = std::make_unique<uint8_t[]>(width * height * components);
  canvas = Canvas(memory.get(), width, height, components);
}

OptError<>
writeImage(const char* path, const Canvas& canvas) {
  MemoryCanvas test(canvas.width(), canvas.height(), 1);

  canvas.forEach([&](auto x, auto y, auto pixel) {
    test.canvas.setPixel({ x, y }, fromRGB565(pixel));
  });

  if (!stbi_write_png(path,
                      test.canvas.width(),
                      test.canvas.height(),
                      test.canvas.components(),
                      test.canvas.getMemory(),
                      test.canvas.lineSize())) {
    return Error::make("Error writing image");
  }
  return {};
}

} // namespace rmlib
