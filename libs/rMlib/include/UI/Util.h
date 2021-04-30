#pragma once

#include <FrameBuffer.h>
#include <MathUtil.h>
#include <functional>

namespace rmlib {

using Callback = std::function<void()>;

enum class Axis { Horizontal, Vertical };

struct Insets {
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;

  constexpr static Insets all(int size) { return { size, size, size, size }; }

  constexpr int horizontal() const { return left + right; }
  constexpr int vertical() const { return top + bottom; }

  constexpr Rect shrink(const Rect& rect) const {
    return Rect{ { rect.topLeft.x + left, rect.topLeft.y + top },
                 { rect.bottomRight.x - right, rect.bottomRight.y - bottom } };
  }

  constexpr bool operator==(const Insets& other) const {
    return top == other.top && bottom == other.bottom && left == other.left &&
           right == other.right;
  }

  constexpr bool operator!=(const Insets& other) const {
    return !(*this == other);
  }
};

struct Constraints {
  static constexpr auto unbound = std::numeric_limits<int>::max();

  Size min;
  Size max;

  constexpr bool hasBoundedWidth() const { return max.width != unbound; }
  constexpr bool hasBoundedHeight() const { return max.height != unbound; }
  constexpr bool hasFiniteWidth() const { return min.width != unbound; }
  constexpr bool hasFiniteHeight() const { return min.height != unbound; }

  constexpr bool contain(Size size) const {
    return min.width <= size.width && size.width <= max.width &&
           min.height <= size.height && size.height <= max.height;
  }

  constexpr Constraints inset(Insets insets) const {
    const auto minHorizontal = std::max(0, min.width - insets.horizontal());
    const auto minVertical = std::max(0, min.height - insets.vertical());

    const auto maxHorizontal =
      hasBoundedWidth()
        ? std::max(minHorizontal, max.width - insets.horizontal())
        : unbound;
    const auto maxVertical =
      hasBoundedHeight() ? std::max(minVertical, max.height - insets.vertical())
                         : unbound;

    return Constraints{ { minHorizontal, minVertical },
                        { maxHorizontal, maxVertical } };
  }

  constexpr Size expand(Size size, Insets insets) const {
    const auto newWidth = size.width + insets.horizontal();
    const auto newHeight = size.height + insets.vertical();

    return Size{ std::min(newWidth, max.width),
                 std::min(newHeight, max.height) };
  }
};

struct UpdateRegion {
  constexpr UpdateRegion() : region(), waveform(fb::Waveform::DU) {}

  constexpr UpdateRegion(Rect region)
    : region(region), waveform(fb::Waveform::GC16Fast) {}

  constexpr UpdateRegion(Rect region, fb::Waveform waveform)
    : region(region), waveform(waveform) {}

  constexpr UpdateRegion(Rect region,
                         fb::Waveform waveform,
                         fb::UpdateFlags flags)
    : region(region), waveform(waveform), flags(flags) {}

  Rect region = { { 0, 0 }, { 0, 0 } };
  fb::Waveform waveform = fb::Waveform::GC16Fast;
  fb::UpdateFlags flags = fb::UpdateFlags::None;

  constexpr UpdateRegion& operator|=(const UpdateRegion& other) {
    if (other.waveform == fb::Waveform::GC16) {
      waveform = other.waveform;
    } else if (other.waveform == fb::Waveform::GC16Fast &&
               waveform == fb::Waveform::DU) {
      waveform = other.waveform;
    }

    if (region.empty()) {
      region = other.region;
    } else if (!other.region.empty()) {
      region |= other.region;
    }

    flags = static_cast<fb::UpdateFlags>(flags | other.flags);

    return *this;
  }
};

constexpr UpdateRegion
operator|(UpdateRegion a, const UpdateRegion& b) {
  a |= b;
  return a;
}

class CachedBool {
public:
  template<typename Fn>
  bool getOrSetTo(Fn&& fn) {
    if (value == UNSET) {
      const auto newVal = fn();
      value = newVal ? TRUE : FALSE;
      return newVal;
    }

    return value == TRUE;
  }

  void reset() { value = UNSET; }

private:
  enum { UNSET, TRUE, FALSE } value = UNSET;
};

} // namespace rmlib
