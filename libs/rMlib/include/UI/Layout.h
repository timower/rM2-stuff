#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

namespace rmlib {
template<typename Child>
class Center;

template<typename Child>
class CenterRenderObject : public SingleChildRenderObject<Center<Child>> {
public:
  using SingleChildRenderObject<Center<Child>>::SingleChildRenderObject;

  // CenterRenderObject(const Center<Child>& widget)
  //   :
  //   SingleChildRenderObject<Center<Child>>(widget.child.createRenderObject())
  //   , widget(&widget) {}

  Size doLayout(const Constraints& constraints) override {
    childSize = this->child->layout(Constraints{ { 0, 0 }, constraints.max });

    auto result = constraints.max;
    if (!constraints.hasBoundedWidth()) {
      result.width = childSize.width;
    }
    if (!constraints.hasBoundedHeight()) {
      result.height = childSize.height;
    }

    return result;
  }

  void update(const Center<Child>& newWidget) {
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

protected:
  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    const auto xOffset = (rect.width() - childSize.width) / 2;
    const auto yOffset = (rect.height() - childSize.height) / 2;

    const auto topLeft = rect.topLeft + rmlib::Point{ xOffset, yOffset };
    const auto bottomRight = topLeft + childSize.toPoint();
    return this->child->draw(rmlib::Rect{ topLeft, bottomRight }, canvas);
  }

private:
  Size childSize;
};

template<typename Child>
class Center : public Widget<CenterRenderObject<Child>> {
private:
public:
  Center(Child child) : child(std::move(child)) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<CenterRenderObject<Child>>(*this);
  }

  Child child;
};

template<class Child>
class Padding;

template<typename Child>
class PaddingRenderObject : public SingleChildRenderObject<Padding<Child>> {
public:
  using SingleChildRenderObject<Padding<Child>>::SingleChildRenderObject;
  // PaddingRenderObject(const Padding<Child>& widget)
  //   : SingleChildRenderObject(widget.child.createRenderObject())
  //   , widget(&widget) {}

  void update(const Padding<Child>& newWidget) {
    if (/*newWidget.insets != widget->insets*/ false) {
      this->markNeedsLayout();
      this->markNeedsDraw();
    }
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    const auto childConstraints = constraints.inset(this->widget->insets);
    const auto childSize = this->child->layout(childConstraints);
    return constraints.expand(childSize, this->widget->insets);
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    const auto childRect = this->widget->insets.shrink(rect);
    auto childRegion = this->child->draw(childRect, canvas);

    return childRegion;
  }
};

template<class Child>
class Padding : public Widget<PaddingRenderObject<Child>> {
private:
public:
  Padding(Child child, Insets insets)
    : child(std::move(child)), insets(insets) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<PaddingRenderObject<Child>>(*this);
  }

  Child child;
  Insets insets;
};

template<typename Child>
class Border;

template<typename Child>
class BorderRenderObject : public SingleChildRenderObject<Border<Child>> {
public:
  using SingleChildRenderObject<Border<Child>>::SingleChildRenderObject;
  // BorderRenderObject(const Border<Child>& widget)
  //   : SingleChildRenderObject(widget.child.createRenderObject())
  //   , widget(&widget) {}

  void update(const Border<Child>& newWidget) {
    if (this->widget->size != newWidget.size) {
      this->markNeedsLayout();
      this->markNeedsDraw();
    }

    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    const auto childConstraints = constraints.inset(this->widget->size);
    const auto childSize = this->child->layout(childConstraints);
    const auto newSize = constraints.expand(childSize, this->widget->size);

    if (newSize != this->getSize()) {
      this->markNeedsDraw();
    }

    return newSize;
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    auto result = this->child->draw(this->widget->size.shrink(rect), canvas);

    if (this->isFullDraw()) {
      const auto drawLine = [&canvas](Point a, Point b, Point dir, int size) {
        for (int i = 0; i < size; i++) {
          canvas.drawLine(a, b, black);
          a += dir;
          b += dir;
        }
      };

      drawLine(rect.topLeft,
               { rect.bottomRight.x, rect.topLeft.y },
               { 0, 1 },
               this->widget->size.top);
      drawLine(rect.topLeft,
               { rect.topLeft.x, rect.bottomRight.y },
               { 1, 0 },
               this->widget->size.left);
      drawLine({ rect.bottomRight.x, rect.topLeft.y },
               rect.bottomRight,
               { -1, 0 },
               this->widget->size.right);
      drawLine({ rect.topLeft.x, rect.bottomRight.y },
               rect.bottomRight,
               { 0, -1 },
               this->widget->size.bottom);

      result |= UpdateRegion{ rect, rmlib::fb::Waveform::DU };
    }

    return result;
  }
};

template<typename Child>
class Border : public Widget<BorderRenderObject<Child>> {
private:
public:
  Border(Child child, Insets size) : child(std::move(child)), size(size) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<BorderRenderObject<Child>>(*this);
  }

  Child child;
  Insets size;
};

template<class Child>
class Sized;

template<typename Child>
class SizedRenderObject : public SingleChildRenderObject<Sized<Child>> {
public:
  using SingleChildRenderObject<Sized<Child>>::SingleChildRenderObject;
  // SizedRenderObject(const Sized<Child>& widget)
  //   : SingleChildRenderObject(widget.child.createRenderObject())
  //   , widget(&widget) {}

  void update(const Sized<Child>& newWidget) {
    if (newWidget.width != this->widget->width ||
        newWidget.height != this->widget->height) {
      this->markNeedsLayout();
      this->markNeedsDraw();
    }
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    const auto& w = this->widget->width;
    const auto& h = this->widget->height;

    const auto childConstraints = Constraints{
      { w.has_value()
          ? std::clamp(*w, constraints.min.width, constraints.max.width)
          : constraints.min.width,
        h.has_value()
          ? std::clamp(*h, constraints.min.height, constraints.max.height)
          : constraints.min.height },
      { w.has_value()
          ? std::clamp(*w, constraints.min.width, constraints.max.width)
          : constraints.max.width,
        h.has_value()
          ? std::clamp(*h, constraints.min.height, constraints.max.height)
          : constraints.max.height }
    };

    const auto childSize = this->child->layout(childConstraints);
    return childSize;
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    return this->child->draw(rect, canvas);
  }
};

template<class Child>
class Sized : public Widget<SizedRenderObject<Child>> {
private:
public:
  Sized(Child child, std::optional<int> width, std::optional<int> height)
    : child(std::move(child)), width(width), height(height) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<SizedRenderObject<Child>>(*this);
  }

  Child child;
  std::optional<int> width;
  std::optional<int> height;
};

template<typename Child>
class ClearedRenderObject;

template<typename Child>
class Cleared : public Widget<ClearedRenderObject<Child>> {
private:
public:
  Cleared(Child child, int color = white)
    : child(std::move(child)), color(color) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<ClearedRenderObject<Child>>(*this);
  }

  Child child;
  int color;
};

template<typename Child>
class ClearedRenderObject : public SingleChildRenderObject<Cleared<Child>> {
public:
  using SingleChildRenderObject<Cleared<Child>>::SingleChildRenderObject;
  // ClearedRenderObject(const Cleared<Child>& widget)
  //   : SingleChildRenderObject(widget.child.createRenderObject())
  //   , widget(&widget) {}

  void update(const Cleared<Child>& newWidget) {
    if (newWidget.color != this->widget->color) {
      this->markNeedsDraw();
    }
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    return this->child->layout(constraints);
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    auto region = UpdateRegion{};

    if (this->isFullDraw()) {
      canvas.set(rect, this->widget->color);
      region = UpdateRegion{ rect };
    }

    return region | this->child->draw(rect, canvas);
  }
};

template<typename Child>
class PositionedRenderObject;

template<typename Child>
class Positioned : public Widget<PositionedRenderObject<Child>> {
private:
public:
  Positioned(Child child, Point position)
    : child(std::move(child)), position(position) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<PositionedRenderObject<Child>>(*this);
  }

  Child child;
  Point position;
};

template<typename Child>
class PositionedRenderObject
  : public SingleChildRenderObject<Positioned<Child>> {
public:
  using SingleChildRenderObject<Positioned<Child>>::SingleChildRenderObject;
  // PositionedRenderObject(const Positioned<Child>& widget)
  //   : SingleChildRenderObject(widget.child.createRenderObject())
  //   , widget(&widget) {}

  void update(const Positioned<Child>& newWidget) {
    if (newWidget.position != this->widget->position) {
      this->markNeedsLayout();

      // Hack to no clear whole rect only child rect
      RenderObject::markNeedsDraw(/* full */ false);
      this->child->markNeedsDraw(true);
    }
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    const auto newConstraints =
      Constraints{ { 0, 0 },
                   { constraints.max.width - this->widget->position.x,
                     constraints.max.height - this->widget->position.y } };

    childSize = this->child->layout(newConstraints);

    auto result = constraints.max;
    if (!constraints.hasBoundedWidth()) {
      result.width = childSize.width;
    }
    if (!constraints.hasBoundedHeight()) {
      result.height = childSize.height;
    }

    return result;
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    const auto topLeft = rect.topLeft + this->widget->position;
    const auto bottomRight = topLeft + childSize.toPoint();
    return this->child->draw({ topLeft, bottomRight }, canvas);
  }

private:
  Size childSize;
};

template<typename Child>
auto
Container(Child child,
          Insets padding = {},
          Insets border = Insets::all(0),
          Insets margin = {}) {
  return Padding(Border(Padding(child, padding), border), margin);
}

} // namespace rmlib
