#pragma once

#include "MathUtil.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <type_traits>

namespace rmlib {

struct Canvas {
  int lineSize() const { return width * components; }
  int totalSize() const { return lineSize() * height; }

  Rect rect() const { return { { 0, 0 }, { width - 1, height - 1 } }; }

  int getPixel(int x, int y) {
    int result = 0;
    auto* pixel = &memory[y * lineSize() + x * components];
    memcpy(&result, pixel, components);
    return result;
  }

  template<typename Func>
  void transform(Func&& f, Rect r) {
    for (int y = r.topLeft.y; y <= r.bottomRight.y; y++) {
      for (int x = r.topLeft.x; x <= r.bottomRight.x; x++) {
        auto* pixel = &memory[y * lineSize() + x * components];
        int value = 0;

        memcpy(&value, pixel, components);
        value = f(x, y, value);
        memcpy(pixel, &value, components);
      }
    }
  }

  template<typename Func>
  void transform(Func&& f) {
    transform(std::forward<Func>(f), rect());
  }

  template<typename Func>
  void forEach(Func&& func, Rect r) const {
    for (int y = r.topLeft.y; y <= r.bottomRight.y; y++) {
      for (int x = r.topLeft.x; x <= r.bottomRight.x; x++) {
        auto* pixel = &memory[y * lineSize() + x * components];
        int value = 0;

        memcpy(&value, pixel, components);
        func(x, y, value);
      }
    }
  }

  template<typename Func>
  void forEach(Func&& func) const {
    forEach(std::forward<Func>(func), rect());
  }

  void set(Rect rect, int value) {
    transform([value](int x, int y, int val) { return value; }, rect);
  }
  void set(int value) { set(rect(), value); }

  uint8_t* memory = nullptr;

  int width;
  int height;
  int components;
};

struct ImageCanvas {
  static std::optional<ImageCanvas> load(const char* path, int components = 0);

  ImageCanvas(ImageCanvas&& other) : canvas(other.canvas) {
    other.canvas.memory = nullptr;
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

template<typename Func>
void
transform(Canvas& dest,
          const Point& destOffset,
          const Canvas& src,
          const Rect& srcRect,
          Func&& f) {
  for (int y = srcRect.topLeft.y; y <= srcRect.bottomRight.y; y++) {
    for (int x = srcRect.topLeft.x; x <= srcRect.bottomRight.x; x++) {
      // TODO: correct offsets
      auto* srcPixel = &src.memory[y * src.lineSize() + x * src.components];
      auto* destPixel = &dest.memory[(y + destOffset.y) * dest.lineSize() +
                                     (x + destOffset.x) * dest.components];
      int value = 0;
      memcpy(&value, srcPixel, src.components);
      value = f(x, y, value);
      memcpy(destPixel, &value, dest.components);
    }
  }
}

} // namespace rmlib
