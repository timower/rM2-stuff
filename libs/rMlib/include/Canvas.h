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

constexpr auto default_text_size = 48;

constexpr auto white = 0xFFFF;
constexpr auto black = 0x0;

// Returns a glyph for the given codepoint.
bool
getGlyph(uint32_t code, uint8_t* bitmap, int height, int* width);

class Canvas {
public:
  Canvas() = default;
  Canvas(uint8_t* mem, int width, int height, int components)
    : memory(mem)
    , mWidth(width)
    , mHeight(height)
    , mComponents(components)
    , mLineSize(width * components) {}

  Canvas(uint8_t* mem, int width, int height, int stride, int components)
    : memory(mem)
    , mWidth(width)
    , mHeight(height)
    , mComponents(components)
    , mLineSize(stride) {}

  int totalSize() const { return mLineSize * mHeight; }

  Rect rect() const { return { { 0, 0 }, { mWidth - 1, mHeight - 1 } }; }

  template<typename T = uint8_t>
  const T* getPtr(int x, int y) const {
    assert(sizeof(T) == 1 || sizeof(T) == mComponents);
    // NOLINTNEXTLINE
    return reinterpret_cast<T*>(memory + y * mLineSize + x * mComponents);
  }

  template<typename T = uint8_t>
  T* getPtr(int x, int y) {
    assert(sizeof(T) == 1 || sizeof(T) == mComponents);
    // NOLINTNEXTLINE
    return reinterpret_cast<T*>(memory + y * mLineSize + x * mComponents);
  }

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
    memcpy(pixel, &val, mComponents);
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

  static Point getTextSize(std::string_view text, int size = default_text_size);

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
  int width() const { return mWidth; }
  int height() const { return mHeight; }
  int lineSize() const { return mLineSize; }
  uint8_t* getMemory() const { return memory; }

  Canvas subCanvas(Rect rect) {
    return { getPtr(rect.topLeft.x, rect.topLeft.y),
             rect.width(),
             rect.height(),
             mLineSize,
             mComponents };
  }

  bool operator==(const Canvas& other) const {
    if (memory == other.memory) {
      return true;
    }

    if (memory == nullptr || other.memory == nullptr) {
      return false;
    }

    if (mWidth != other.mWidth) {
      return false;
    }

    if (mHeight != other.mHeight) {
      return false;
    }

    if (mComponents != other.mComponents) {
      return false;
    }

    for (int y = 0; y < mHeight; y++) {
      const auto* line = getPtr(0, y);
      const auto* otherLine = other.getPtr(0, y);
      if (memcmp(line, otherLine, mWidth * mComponents) != 0) {
        std::cout << "Diff at line " << y << "\n";
        return false;
      }
    }

    return true;
  }

  bool operator!=(const Canvas& other) const { return !(*this == other); }

private:
  template<typename ValueType, typename Func>
  void transformImpl(const Func& f, Rect r) {
    for (int y = r.topLeft.y; y <= r.bottomRight.y; y++) {
      for (int x = r.topLeft.x; x <= r.bottomRight.x; x++) {
        auto* ptr = getPtr<ValueType>(x, y);
        *ptr = f(x, y, *ptr);
      }
    }
  }

  uint8_t* memory = nullptr;

  int mWidth = 0;
  int mHeight = 0;
  int mComponents = 0;
  int mLineSize = 0;
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

inline void
copy(Canvas& dest,
     const Point& destOffset,
     const Canvas& src,
     const Rect& srcRect) {
  assert(dest.components() == src.components());
  assert(src.rect().contains(srcRect));
  assert(dest.rect().contains(srcRect + destOffset));

  for (int y = srcRect.topLeft.y; y <= srcRect.bottomRight.y; y++) {
    const auto* srcPixel = src.getPtr<>(srcRect.topLeft.x, y);
    auto* destPixel =
      dest.getPtr<>(destOffset.x, (y - srcRect.topLeft.y + destOffset.y));
    memcpy(destPixel, srcPixel, srcRect.width() * src.components());
  }
}

template<typename Func>
void
transform(Canvas& dest,
          const Point& destOffset,
          const Canvas& src,
          const Rect& srcRect,
          const Func& f) {
  assert(src.rect().contains(srcRect));
  assert(dest.rect().contains(srcRect + destOffset));

  for (int y = srcRect.topLeft.y; y <= srcRect.bottomRight.y; y++) {
    for (int x = srcRect.topLeft.x; x <= srcRect.bottomRight.x; x++) {
      const auto* srcPixel = src.getPtr<>(x, y);
      // TODO: correct offsets
      auto* destPixel = dest.getPtr<>((x + destOffset.x - srcRect.topLeft.x),
                                      (y + destOffset.y - srcRect.topLeft.y));
      int value = 0;
      memcpy(&value, srcPixel, src.components());
      value = f(x, y, value);
      memcpy(destPixel, &value, dest.components());
    }
  }
}

OptError<>
writeImage(const char* path, const Canvas& canvas);

} // namespace rmlib
