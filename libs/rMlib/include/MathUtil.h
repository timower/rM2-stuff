#pragma once

#include <array>
#include <cmath>
#include <ostream>

namespace rmlib {

struct Point {
  int x = 0;
  int y = 0;

  constexpr Point& operator*=(int val) {
    x *= val;
    y *= val;
    return *this;
  }

  constexpr Point& operator-=(const Point& o) {
    x -= o.x;
    y -= o.y;
    return *this;
  }

  constexpr Point& operator+=(const Point& o) {
    x += o.x;
    y += o.y;
    return *this;
  }

  constexpr Point& operator/=(int val) {
    x /= val;
    y /= val;
    return *this;
  }

  constexpr int lengthSqrt() const { return x * x + y * y; }
};

constexpr bool
operator==(const Point& a, const Point& b) {
  return a.x == b.x && a.y == b.y;
}

constexpr bool
operator!=(const Point& a, const Point& b) {
  return a.x != b.x || a.y != b.y;
}

constexpr Point
operator-(Point a, const Point& b) {
  a -= b;
  return a;
}

constexpr Point
operator+(Point a, const Point& b) {
  a += b;
  return a;
}

constexpr Point
operator*(Point a, int b) {
  a *= b;
  return a;
}

constexpr Point
operator/(Point a, int b) {
  a /= b;
  return a;
}

template<typename T>
std::basic_ostream<char, T>&
operator<<(std::basic_ostream<char, T>& os, const Point& p) {
  os << "{ " << p.x << ", " << p.y << " }";
  return os;
}

struct Transform {
  std::array<std::array<float, 2>, 2> matrix = { { { 1, 0 }, { 0, 1 } } };
  Point offset;

  static constexpr Transform identity() { return Transform{}; }

  static constexpr Transform scale(float sx, float sy) {
    return Transform{ { { { sx, 0 }, { 0, sy } } }, { 0, 0 } };
  }

  static constexpr Transform translate(Point t) {
    return Transform{ { { { 1, 0 }, { 0, 1 } } }, t };
  }

  static Transform rotate(float radians) {
    auto cs = cosf(radians);
    auto sn = sinf(radians);
    return Transform{ { { { cs, -sn }, { sn, cs } } }, { 0, 0 } };
  }
};

constexpr Point
operator*(const Transform& t, const Point& p) {
  Point r;
  auto px = float(p.x);
  auto py = float(p.y);
  r.x = int(t.matrix[0][0] * px + t.matrix[0][1] * py + float(t.offset.x));
  r.y = int(t.matrix[1][0] * px + t.matrix[1][1] * py + float(t.offset.y));
  return r;
}

constexpr Transform
operator*(const Transform& lhs, const Transform& rhs) {
  Transform r;

  r.matrix[0][0] =
    lhs.matrix[0][0] * rhs.matrix[0][0] + lhs.matrix[0][1] * rhs.matrix[1][0];
  r.matrix[0][1] =
    lhs.matrix[0][0] * rhs.matrix[0][1] + lhs.matrix[0][1] * rhs.matrix[1][1];
  r.matrix[1][0] =
    lhs.matrix[1][0] * rhs.matrix[0][0] + lhs.matrix[1][1] * rhs.matrix[1][0];
  r.matrix[1][1] =
    lhs.matrix[1][0] * rhs.matrix[0][1] + lhs.matrix[1][1] * rhs.matrix[1][1];

  r.offset = lhs * rhs.offset;

  return r;
}

struct Size {
  int width;
  int height;

  constexpr rmlib::Point toPoint() const { return { width - 1, height - 1 }; }

  friend constexpr bool operator==(const Size& lhs, const Size& rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height;
  }

  friend constexpr bool operator!=(const Size& lhs, const Size& rhs) {
    return !(lhs == rhs);
  }

  constexpr Size& operator-=(const Size& rhs) {
    width -= rhs.width;
    height -= rhs.height;
    return *this;
  }

  constexpr Size& operator/=(int rhs) {
    width /= rhs;
    height /= rhs;
    return *this;
  }
};

constexpr Size
operator-(Size lhs, const Size& rhs) {
  lhs -= rhs;
  return lhs;
}

constexpr Size
operator/(Size lhs, int rhs) {
  lhs /= rhs;
  return lhs;
}

struct Rect {
  /// These points are inclusive. So both are part of the Rect.
  Point topLeft = { 0, 0 };
  Point bottomRight = { -1, -1 };

  constexpr int width() const { return bottomRight.x - topLeft.x + 1; }
  constexpr int height() const { return bottomRight.y - topLeft.y + 1; }
  constexpr Size size() const { return { width(), height() }; }

  constexpr bool empty() const { return width() == 0 || height() == 0; }

  /// Scale the rect by an integer.
  constexpr Rect& operator*=(int val) {
    topLeft *= val;
    bottomRight *= val;
    return *this;
  }

  constexpr Rect& operator+=(const Point& p) {
    topLeft += p;
    bottomRight += p;
    return *this;
  }

  constexpr Rect& operator|=(const Rect& other) {
    if (other.empty()) {
      return *this;
    }
    if (empty()) {
      *this = other;
      return *this;
    }

    topLeft = { std::min(topLeft.x, other.topLeft.x),
                std::min(topLeft.y, other.topLeft.y) };
    bottomRight = { std::max(bottomRight.x, other.bottomRight.x),
                    std::max(bottomRight.y, other.bottomRight.y) };
    return *this;
  }

  constexpr bool contains(Point p) const {
    return topLeft.x <= p.x && p.x <= bottomRight.x && topLeft.y <= p.y &&
           p.y <= bottomRight.y;
  }

  constexpr bool contains(const Rect& r) const {
    if (empty() && r.empty() && r.topLeft == topLeft) {
      return true;
    }
    return contains(r.topLeft) && (r.empty() || contains(r.bottomRight));
  }

  constexpr Rect align(Size size, float horizontal, float vertical) const {
    // assert(0 <= horizontal && horizontal <= 1.0);
    // assert(0 <= vertical && vertical <= 1.0);
    const auto diff = this->size() - size;
    const auto dx = float(diff.width);
    const auto dy = float(diff.height);
    const auto offset = Point{ int(dx * horizontal), int(dy * vertical) };
    const auto start = topLeft + offset;
    return Rect{ start, start + size.toPoint() };
  }

  constexpr Point center() const { return (topLeft + bottomRight) / 2; }
};

static_assert(Rect{}.width() == 0);
static_assert(Rect{}.height() == 0);

constexpr Rect
operator&(const Rect& a, const Rect& b) {
  return Rect{ { std::max(a.topLeft.x, b.topLeft.x),
                 std::max(a.topLeft.y, b.topLeft.y) },
               { std::min(a.bottomRight.x, b.bottomRight.x),
                 std::min(a.bottomRight.y, b.bottomRight.y) } };
}

constexpr Rect
operator|(Rect a, const Rect& b) {
  a |= b;
  return a;
}

constexpr Rect
operator+(Rect r, const Point& p) {
  r += p;
  return r;
}

constexpr Rect
operator+(const Point& p, Rect r) {
  r += p;
  return r;
}

template<typename T>
std::basic_ostream<char, T>&
operator<<(std::basic_ostream<char, T>& os, const Rect& r) {
  os << "{ " << r.topLeft << ", " << r.bottomRight << " }";
  return os;
}

template<typename T>
std::basic_ostream<char, T>&
operator<<(std::basic_ostream<char, T>& os, const Size& p) {
  os << "{ " << p.width << ", " << p.height << " }";
  return os;
}

enum class Rotation {
  None = 0,
  Clockwise = 1,
  Inverted = 2,
  CounterClockwise = 3,
};

inline Rotation
rotate(Rotation a, Rotation b) {
  return static_cast<Rotation>((static_cast<int>(a) + static_cast<int>(b)) % 4);
}

inline Rotation
invert(Rotation rot) {
  switch (rot) {
    case Rotation::None:
      return Rotation::None;
    case Rotation::Clockwise:
      return Rotation::CounterClockwise;
    case Rotation::CounterClockwise:
      return Rotation::Clockwise;
    case Rotation::Inverted:
      return Rotation::Inverted;
  };
}

inline Point
rotate(const Size& size, Rotation rotation, const Point& p) {
  const auto sizePoint = size.toPoint();
  switch (rotation) {
    case Rotation::None:
      return p;
    case Rotation::Clockwise:
      return { p.y, sizePoint.x - p.x };
    case Rotation::CounterClockwise:
      return { sizePoint.y - p.y, p.x };
    case Rotation::Inverted:
      return { sizePoint.x - p.x, sizePoint.y - p.y };
  };
}

inline Size
rotate(Rotation rotation, const Size& s) {
  switch (rotation) {
    case Rotation::None:
    case Rotation::Inverted:
      return s;
    case Rotation::Clockwise:
    case Rotation::CounterClockwise:
      return { s.height, s.width };
  };
}

inline Rect
rotate(const Size& size, Rotation rotation, const Rect& r) {
  const auto a = rotate(size, rotation, r.topLeft);
  const auto b = rotate(size, rotation, r.bottomRight);
  return { { std::min(a.x, b.x), std::min(a.y, b.y) },
           { std::max(a.x, b.x), std::max(a.y, b.y) } };
}

} // namespace rmlib
