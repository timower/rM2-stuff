#include "Canvas.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <array>
#include <codecvt>
#include <iostream>
#include <locale>
#include <vector>

namespace rmlib {

namespace {
#ifdef EMULATE
constexpr auto font_path = "/usr/share/fonts/TTF/DejaVuSansMono.ttf";
#else
constexpr auto font_path = "/usr/share/fonts/ttf/noto/NotoMono-Regular.ttf";
#endif

stbtt_fontinfo*
getFont() {
  static auto* font = [] {
    static std::array<uint8_t, 24 << 20> fontBuffer = { 0 };
    static stbtt_fontinfo font;

    auto* fp = fopen(font_path, "r"); // NOLINT
    if (fp == nullptr) {
      std::cerr << "Error opening font\n";
    }
    auto len = fread(fontBuffer.data(), 1, fontBuffer.size(), fp);
    if (len == static_cast<decltype(len)>(-1)) {
      std::cerr << "Error reading font\n";
    }

    stbtt_InitFont(&font, fontBuffer.data(), 0);
    fclose(fp); // NOLINT
    return &font;
  }();

  return font;
}
} // namespace

bool
getGlyph(uint32_t code, uint8_t* bytemap, int height, int* width) {
  static std::vector<uint8_t> tmpBuf;

  const auto* font = getFont();
  auto scale = stbtt_ScaleForPixelHeight(font, float(height));

  int ascent = 0;
  stbtt_GetFontVMetrics(font, &ascent, nullptr, nullptr);
  auto baseline = static_cast<int>(float(ascent) * scale);

  int advance = 0;
  int lsb = 0;
  stbtt_GetCodepointHMetrics(font, code, &advance, &lsb);
  *width = float(advance) * scale;

  int x0 = 0;
  int x1 = 0;
  int y0 = 0;
  int y1 = 0;
  stbtt_GetCodepointBitmapBox(font, code, scale, scale, &x0, &y0, &x1, &y1);

  int w = x1 - x0;
  int h = y1 - y0;
  int size = w * h;
  if (size_t(size) > tmpBuf.size()) {
    tmpBuf.resize(size);
  }

  stbtt_MakeCodepointBitmap(font, tmpBuf.data(), w, h, w, scale, scale, code);

  // Draw the bitmap to canvas.
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      auto pixel = tmpBuf[y * w + x];

      auto memY = baseline + y0 + y;
      auto memX = x0 + x;

      if (memX < 0 || memX > 32) {
        std::cerr << "Error memx: " << code << " memX: " << memX
                  << " x0: " << x0 << " x: " << x << " width: " << *width
                  << " w: " << w << std::endl;
        continue;
      }

      assert(0 <= memX && memX < 32);
      assert(0 <= memY && memY < 32);

      bytemap[memY * 32 + memX] = pixel;
      // if (pixel / 16 == 0xf) {
      //   bitmap[memY] |= (1 << memX);
      // }
    }
  }

  return true;
}

Point
Canvas::getTextSize(std::string_view text, int size) {
  const auto* font = getFont();
  auto scale = stbtt_ScaleForPixelHeight(font, float(size));

  int ascent = 0;
  stbtt_GetFontVMetrics(font, &ascent, nullptr, nullptr);
  auto baseline = static_cast<int>(float(ascent) * scale);

  auto utf32 =
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>{}.from_bytes(
      text.data());

  int maxY = 0;
  float xpos = 0;
  for (int ch = 0; utf32[ch] != 0; ch++) {
    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(font, utf32[ch], &advance, &lsb);

    int x0 = 0;
    int x1 = 0;
    int y0 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBox(
      font, utf32[ch], scale, scale, &x0, &y0, &x1, &y1);

    auto y = baseline + y1;
    maxY = std::max(maxY, y);

    xpos += float(advance) * scale;
    if (utf32[ch + 1] != 0) {
      xpos +=
        scale *
        float(stbtt_GetCodepointKernAdvance(font, utf32[ch], utf32[ch + 1]));
    }
  }

  return { static_cast<int>(xpos), maxY };
}

void
Canvas::drawText(std::string_view text, Point location, int size) { // NOLINT
  const auto* font = getFont();
  auto scale = stbtt_ScaleForPixelHeight(font, float(size));

  int ascent = 0;
  stbtt_GetFontVMetrics(font, &ascent, nullptr, nullptr);
  auto baseline = static_cast<int>(float(ascent) * scale);

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

    int w = x1 - x0;
    int h = y1 - y0;
    int size = w * h;
    if (static_cast<unsigned>(size) > textBuffer.size()) {
      textBuffer.resize(size);
    }

    stbtt_MakeCodepointBitmapSubpixel(
      font, textBuffer.data(), w, h, w, scale, scale, xShift, 0, utf32[ch]);

    // Draw the bitmap to canvas.
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        auto pixel = 0xff - textBuffer[y * w + x];
        uint16_t pixel565 =
          (pixel >> 3) | ((pixel >> 2) << 5) | ((pixel >> 3) << 11);

        auto memY = location.y + baseline + y0 + y;
        auto memX = location.x + static_cast<int>(xpos) + x0 + x;

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
Canvas::drawLine(Point start, Point end, int val) {
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

std::optional<ImageCanvas>
ImageCanvas::load(const char* path, int components) {
  int width;
  int height;
  int imgComponents;
  auto* mem = stbi_load(path, &width, &height, &imgComponents, components);
  if (mem == nullptr) {
    return std::nullopt;
  }

  if (components != 0) {
    imgComponents = components;
  }

  Canvas result(mem, width, height, imgComponents);
  return ImageCanvas{ result };
}

std::optional<ImageCanvas>
ImageCanvas::load(uint8_t* data, int size, int components) {
  int width;
  int height;
  int imgComponents;
  auto* mem = stbi_load_from_memory(
    data, size, &width, &height, &imgComponents, components);
  if (mem == nullptr) {
    return std::nullopt;
  }

  if (components != 0) {
    imgComponents = components;
  }

  Canvas result(mem, width, height, imgComponents);
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

} // namespace rmlib
