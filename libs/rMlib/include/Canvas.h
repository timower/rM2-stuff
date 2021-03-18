#pragma once

#include "MathUtil.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

namespace rmlib {

constexpr auto default_text_size = 24;

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

  int getPixel(int x, int y) {
    assert(rect().contains(Point{ x, y }));
    int result = 0;
    auto* pixel = &memory[y * mLineSize + x * mComponents];
    memcpy(&result, pixel, mComponents);
    return result;
  }

  void setPixel(Point p, int val) {
    assert(rect().contains(p));
    auto* pixel = &memory[p.y * mLineSize + p.x * mComponents];
    memcpy(pixel, &val, mComponents);
  }

  template<typename Func>
  void transform(Func&& f, Rect r) {
    assert(rect().contains(r.bottomRight) && rect().contains(r.topLeft));
    for (int y = r.topLeft.y; y <= r.bottomRight.y; y++) {
      for (int x = r.topLeft.x; x <= r.bottomRight.x; x++) {
        auto* pixel = &memory[y * mLineSize + x * mComponents];
        int value = 0;

        memcpy(&value, pixel, mComponents);
        value = f(x, y, value);
        memcpy(pixel, &value, mComponents);
      }
    }
  }

  template<typename Func>
  void transform(Func&& f) {
    transform(std::forward<Func>(f), rect());
  }

  template<typename Func>
  void forEach(Func&& func, Rect r) const {
    assert(rect().contains(r.bottomRight) && rect().contains(r.topLeft));
    for (int y = r.topLeft.y; y <= r.bottomRight.y; y++) {
      for (int x = r.topLeft.x; x <= r.bottomRight.x; x++) {
        auto* pixel = &memory[y * mLineSize + x * mComponents];
        int value = 0;

        memcpy(&value, pixel, mComponents);
        func(x, y, value);
      }
    }
  }

  template<typename Func>
  void forEach(Func&& func) const {
    forEach(std::forward<Func>(func), rect());
  }

  void set(Rect r, int value) {
    assert(rect().contains(r.bottomRight) && rect().contains(r.topLeft));
    transform([value](int x, int y, int val) { return value; }, r);
  }

  void set(int value) { set(rect(), value); }

  static Point getTextSize(std::string_view text, int size = default_text_size);

  void drawText(std::string_view text,
                Point location,
                int size = default_text_size);

  void drawLine(Point start, Point end, int val);

  template<typename T = uint8_t>
  const T* getPtr(int x, int y) const {
    assert(sizeof(T) == 1 || sizeof(T) == mComponents);
    return reinterpret_cast<T*>(memory + y * mLineSize + x * mComponents);
  }

  template<typename T = uint8_t>
  T* getPtr(int x, int y) {
    assert(sizeof(T) == 1 || sizeof(T) == mComponents);
    return reinterpret_cast<T*>(memory + y * mLineSize + x * mComponents);
  }

  int components() const { return mComponents; }
  int width() const { return mWidth; }
  int height() const { return mHeight; }
  int lineSize() const { return mLineSize; }
  uint8_t* getMemory() const { return memory; }

  // members
private:
  uint8_t* memory = nullptr;

  int mWidth = 0;
  int mHeight = 0;
  int mComponents = 0;
  int mLineSize = 0;
};

struct ImageCanvas {
  static std::optional<ImageCanvas> load(const char* path, int components = 0);
  static std::optional<ImageCanvas> load(uint8_t* data,
                                         int size,
                                         int components = 0);

  ImageCanvas(ImageCanvas&& other) : canvas(other.canvas) {
    other.canvas = Canvas{};
  }

  ImageCanvas& operator=(ImageCanvas&& other) {
    release();
    std::swap(other, *this);
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
  MemoryCanvas(const Canvas& other) : MemoryCanvas(other, other.rect()) {}
  MemoryCanvas(const Canvas& other, Rect rect);

  std::unique_ptr<uint8_t[]> memory;
  Canvas canvas;
};

inline void
copy(Canvas& dest,
     const Point& destOffset,
     const Canvas& src,
     const Rect& srcRect) {
  assert(dest.components() == src.components());

  for (int y = srcRect.topLeft.y; y <= srcRect.bottomRight.y; y++) {
    auto* srcPixel = src.getPtr<>(srcRect.topLeft.x, y);
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
          Func&& f) {
  for (int y = srcRect.topLeft.y; y <= srcRect.bottomRight.y; y++) {
    for (int x = srcRect.topLeft.x; x <= srcRect.bottomRight.x; x++) {
      auto* srcPixel = src.getPtr<>(x, y);
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

} // namespace rmlib
