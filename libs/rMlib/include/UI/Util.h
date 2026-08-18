#pragma once

#include <FrameBuffer.h>
#include <MathUtil.h>
#include <functional>
#include <vector>

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
  constexpr bool isBounded() const {
    return hasBoundedHeight() && hasBoundedWidth();
  }

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

  constexpr bool operator==(const Constraints& other) const {
    return min == other.min && max == other.max;
  }

  constexpr bool operator!=(const Constraints& other) const {
    return !(*this == other);
  }
};

inline Constraints
rotate(const Rotation& rotation, const Constraints& c) {
  return { rotate(rotation, c.min), rotate(rotation, c.max) };
}

struct UpdateRegion {
  constexpr UpdateRegion() : region(), waveform(fb::Waveform::DU) {}

  constexpr UpdateRegion(Rect region) : region(region) {}

  constexpr UpdateRegion(Rect region, fb::Waveform waveform)
    : region(region), waveform(waveform) {}

  constexpr UpdateRegion(Rect region,
                         fb::Waveform waveform,
                         fb::UpdateFlags flags)
    : region(region), waveform(waveform), flags(flags) {}

  Rect region = { { 0, 0 }, { 0, 0 } };
  fb::Waveform waveform = fb::Waveform::GC16Fast;
  fb::UpdateFlags flags = fb::UpdateFlags::None;
};

// Applies `fn` to every UpdateRegion appended to `out` since `start` - used
// to rebase/transform/restamp only the regions a subtree just pushed.
template<typename Fn>
void
forEachSince(std::vector<UpdateRegion>& out, std::size_t start, Fn&& fn) {
  for (auto i = start; i < out.size(); i++) {
    fn(out[i]);
  }
}

// Swtcon resolves overlapping updates by keeping whichever was submitted
// last, not the highest-quality one - sort by this before submitting a
// frame's updates so the best-quality region always wins any overlap.
constexpr int
waveformQuality(fb::Waveform waveform) {
  switch (waveform) {
    case fb::Waveform::GC16:
      return 2;
    case fb::Waveform::GC16Fast:
      return 1;
    default:
      return 0;
  }
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
