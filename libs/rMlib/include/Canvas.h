#pragma once

#include "Error.h"
#include "MathUtil.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

#include <iostream>

namespace rmlib {
namespace fb {
struct FrameBuffer;
}

constexpr auto default_text_size = 48;

constexpr auto white = 0xFFFF;
constexpr auto black = 0x0;

constexpr uint16_t
greyToRGB565(uint8_t grey) {
  // NOLINTNEXTLINE
  return (grey >> 3) | ((grey >> 2) << 5) | ((grey >> 3) << 11);
}

constexpr uint8_t
greyFromRGB565(uint16_t rgb) {
  // uint8_t r = (rgb & 0x1f) << 3;
  // NOLINTNEXTLINE
  uint8_t g = ((rgb >> 5) & 0x3f) << 2;
  // uint8_t b = ((rgb >> 11) & 0x1f) << 3;

  // Only use g for now, as it has the most bit depth.
  return g;
}

// Returns a glyph for the given codepoint.
bool
getGlyph(uint32_t code, uint8_t* bitmap, int height, int* width);

// TODO: drop compoments, hardcode to 2 / uint16_t in rgb565 format.
class Canvas {
public:
  Canvas() = default;
  Canvas(uint8_t* mem, int width, int height, int components)
    : mMemory(mem)
    , mLineSize(width * components)
    , mComponents(components)
    , mSize(Size{ width, height }) {}

  int totalSize() const { return mLineSize * numLines(); }

  Rect rect() const { return { { 0, 0 }, mSize.toPoint() }; }

  int getPixel(int x, int y) const {
    assert(rect().contains(Point{ x, y }));
    int result = 0;
    const auto* pixel = getPtr(x, y);
    memcpy(&result, pixel, mComponents);
    return result;
  }

  void setPixel(Point p, int val) {
    assert(rect().contains(p));
    auto* pixel = getPtr(p.x, p.y);
    switch (mComponents) {
      case 2:
        *(uint16_t*)pixel = (uint16_t)val;
        break;
      default:
        memcpy(pixel, &val, mComponents);
        break;
    }
  }

  template<typename Func>
  void transform(Func&& f, Rect r) {
    assert(rect().contains(r));
    switch (mComponents) {
      case 1:
        transformImpl<uint8_t>(std::forward<Func>(f), r);
        break;
      case 2:
        transformImpl<uint16_t>(std::forward<Func>(f), r);
        break;
      case 3:
        assert(false && "TODO");
        break;
      case 4:
        transformImpl<uint32_t>(std::forward<Func>(f), r);
        break;
    }
  }

  template<typename Func>
  void transform(Func&& f) {
    transform(std::forward<Func>(f), rect());
  }

  template<typename Func>
  void forEach(const Func& func, Rect r) const {
    assert(rect().contains(r.bottomRight) && rect().contains(r.topLeft));
    for (int y = r.topLeft.y; y <= r.bottomRight.y; y++) {
      for (int x = r.topLeft.x; x <= r.bottomRight.x; x++) {
        auto value = getPixel(x, y);
        func(x, y, value);
      }
    }
  }

  template<typename Func>
  void forEach(Func&& func) const {
    forEach(std::forward<Func>(func), rect());
  }

  void set(Rect r, int value) {
    assert(rect().contains(r));
    transform([value](auto x, auto y, auto v) { return value; }, r);
  }

  void set(int value) { set(rect(), value); }

  static Size getTextSize(std::string_view text, int size = default_text_size);

  void drawText(std::string_view text,
                Point location,
                int size = default_text_size,
                int fg = black,
                int bg = white,
                std::optional<Rect> clipRect = std::nullopt);

  void drawLine(Point start, Point end, int val, int thickness = 1);
  void drawDisk(Point center, int radius, int val);

  void drawRectangle(Point topLeft, Point bottomRight, int val) {
    drawLine(topLeft, Point{ bottomRight.x, topLeft.y }, val);
    drawLine(topLeft, Point{ topLeft.x, bottomRight.y }, val);
    drawLine(bottomRight, Point{ bottomRight.x, topLeft.y }, val);
    drawLine(bottomRight, Point{ topLeft.x, bottomRight.y }, val);
  }

  int components() const { return mComponents; }
  int width() const { return mSize.width; }
  int height() const { return mSize.height; }
  Size size() const { return mSize; }
  int lineSize() const { return mLineSize; }

  Rotation rotation() const { return mRotation; }

  Canvas subCanvas(Rect rect, Rotation rot = Rotation::None) {
    const auto localRect = rotate(size(), mRotation, rect);
    const auto topleft = localRect.topLeft;
    const auto newSize = rotate(rot, rect.size());
    return { mMemory + (topleft.y * mLineSize) + (topleft.x * mComponents),
             newSize.width,
             newSize.height,
             mLineSize,
             mComponents,
             rotate(mRotation, rot) };
  }

  bool compare(const Canvas& other) const {
    if (mMemory == nullptr || other.mMemory == nullptr) {
      return false;
    }

    if (mMemory == other.mMemory) {
      return true;
    }

    if (mSize != other.mSize) {
      return false;
    }
    if (mComponents != other.mComponents) {
      return false;
    }

    if (mRotation != other.mRotation) {
      return false;
    }

    for (int n = 0; n < numLines(); n++) {
      const auto* line = getLine(n);
      const auto* otherLine = other.getLine(n);
      if (memcmp(line, otherLine, lineWidth() * mComponents) != 0) {
        std::cout << "Diff at line " << n << "\n";
        return false;
      }
    }

    return true;
  }

  void copy(const Canvas& src) {
    assert(components() == src.components());
    assert(rotation() == src.rotation());
    assert(size() == src.size());

    const auto linewidth = lineWidth();
    for (int n = 0; n < numLines(); n++) {
      const auto* srcLine = src.getLine(n);
      auto* destLine = getLine(n);
      memcpy(destLine, srcLine, linewidth * mComponents);
    }
  }

  OptError<> writeImage(const char* path) const;

  bool operator==(const Canvas& other) const {
    return mMemory == other.mMemory && mSize == other.mSize &&
           mComponents == other.mComponents && mLineSize == other.mLineSize &&
           mRotation == other.mRotation;
  }
  bool operator!=(const Canvas& other) const { return !(*this == other); }

private:
  friend struct ImageCanvas;
  friend struct fb::FrameBuffer;

  Canvas(uint8_t* mem,
         int width,
         int height,
         int stride,
         int components,
         Rotation rot = Rotation::None)
    : mMemory(mem)
    , mLineSize(stride)
    , mComponents(components)
    , mSize(Size{ width, height })
    , mRotation(rot) {}

  uint8_t* memory() const { return mMemory; }

  uint8_t* getLine(int n) const { return mMemory + (n * mLineSize); }
  int lineWidth() const { return rotate(mRotation, mSize).width; }
  int numLines() const { return rotate(mRotation, mSize).height; }

  template<typename T = uint8_t>
  const T* getPtr(int x, int y) const {
    assert(sizeof(T) == 1 || sizeof(T) == mComponents);
    const auto lp = rotate(mSize, mRotation, Point{ x, y });
    // NOLINTNEXTLINE
    return reinterpret_cast<T*>(mMemory + (lp.y * mLineSize) +
                                (lp.x * mComponents));
  }

  template<typename T = uint8_t>
  T* getPtr(int x, int y) {
    assert(sizeof(T) == 1 || sizeof(T) == mComponents);
    const auto lp = rotate(mSize, mRotation, Point{ x, y });
    // NOLINTNEXTLINE
    return reinterpret_cast<T*>(mMemory + (lp.y * mLineSize) +
                                (lp.x * mComponents));
  }

  template<typename ValueType, typename Func>
  void transformImpl(const Func& f, Rect r) {
    for (int y = r.topLeft.y; y <= r.bottomRight.y; y++) {
      for (int x = r.topLeft.x; x <= r.bottomRight.x; x++) {
        auto* ptr = getPtr<ValueType>(x, y);
        *ptr = f(x, y, *ptr);
      }
    }
  }

  uint8_t* mMemory = nullptr;
  int mLineSize = 0;
  int mComponents = 0;
  Size mSize = { 0, 0 };
  Rotation mRotation = Rotation::None;
};

struct ImageCanvas {
  static std::optional<ImageCanvas> loadRaw(const char* path);
  static std::optional<ImageCanvas> load(const char* path,
                                         int background = white);
  static std::optional<ImageCanvas> load(uint8_t* data,
                                         int size,
                                         int background = white);

  ImageCanvas(ImageCanvas&& other) noexcept : canvas(other.canvas) {
    other.canvas = Canvas{};
  }

  ImageCanvas& operator=(ImageCanvas&& other) noexcept {
    release();
    std::swap(other.canvas, this->canvas);
    return *this;
  }

  ImageCanvas(const ImageCanvas&) = delete;
  ImageCanvas& operator=(const ImageCanvas&) = delete;

  ~ImageCanvas() { release(); }

  Canvas canvas;

private:
  ImageCanvas(Canvas canvas) : canvas(std::move(canvas)) {}

  void release();
};

struct MemoryCanvas {
  MemoryCanvas(int width, int height, int components);
  MemoryCanvas(const Canvas& other) : MemoryCanvas(other, other.rect()) {}
  MemoryCanvas(const Canvas& other, Rect rect);

  std::unique_ptr<uint8_t[]> memory; // NOLINT
  Canvas canvas;
};

} // namespace rmlib
