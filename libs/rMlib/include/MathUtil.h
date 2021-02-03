#pragma once

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

template<typename T>
std::basic_ostream<char, T>&
operator<<(std::basic_ostream<char, T>& os, const Point& p) {
  os << "{ " << p.x << ", " << p.y << " }";
  return os;
}

struct Transform {
  float matrix[2][2] = { { 1, 0 }, { 0, 1 } };
  Point offset;

  static constexpr Transform identity() { return Transform{}; }

  static constexpr Transform scale(float sx, float sy) {
    return Transform{ { { sx, 0 }, { 0, sy } }, { 0, 0 } };
  }

  static constexpr Transform translate(Point t) {
    return Transform{ { { 1, 0 }, { 0, 1 } }, t };
  }

  static Transform rotate(float radians) {
    auto cs = cosf(radians);
    auto sn = sinf(radians);
    return Transform{ { { cs, -sn }, { sn, cs } }, { 0, 0 } };
  }
};

constexpr Point
operator*(const Transform& t, const Point& p) {
  Point r;
  r.x = t.matrix[0][0] * p.x + t.matrix[0][1] * p.y + t.offset.x;
  r.y = t.matrix[1][0] * p.x + t.matrix[1][1] * p.y + t.offset.y;
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

struct Rect {
  /// These points are inclusive. So both are part of the Rect.
  Point topLeft;
  Point bottomRight;

  constexpr int width() const { return bottomRight.x - topLeft.x + 1; }
  constexpr int height() const { return bottomRight.y - topLeft.y + 1; }

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

  constexpr bool contains(Point p) const {
    return topLeft.x <= p.x && p.x <= bottomRight.x && topLeft.y <= p.y &&
           p.y <= bottomRight.y;
  }
};

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

namespace static_tests {
static_assert(Transform::identity() * Point{ 4, 10 } == Point{ 4, 10 });
static_assert(Transform::scale(4, 2) * Point{ 4, 10 } == Point{ 16, 20 });
static_assert(Transform::translate({ -1, 2 }) * Point{ 4, 10 } ==
              Point{ 3, 12 });

static_assert((Transform::translate({ -1, 2 }) * Transform::scale(4, 2)) *
                Point{ 4, 10 } ==
              Point{ 15, 22 });
static_assert((Transform::scale(4, 2) * Transform::translate({ -1, 2 })) *
                Point{ 4, 10 } ==
              Point{ 12, 24 });

static_assert(Transform{ { { 0, 1 }, { 1, 0 } }, { 0, 0 } } * Point{ 4, 10 } ==
              Point{ 10, 4 });
} // namespace static_tests

} // namespace rmlib
