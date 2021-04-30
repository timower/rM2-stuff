#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

namespace rmlib {

template<typename... Children>
class Flex;

template<typename... Children>
class FlexRenderObject : public MultiChildRenderObject<Flex<Children...>> {
public:
  FlexRenderObject(const Flex<Children...>& widget)
    : MultiChildRenderObject<Flex<Children...>>(std::apply(
        [](auto&... child) {
          auto array = std::array{ child.createRenderObject()... };
          std::vector<std::unique_ptr<RenderObject>> objs;
          objs.reserve(num_children);
          std::move(array.begin(), array.end(), std::back_inserter(objs));
          return objs;
        },
        widget.children))
    , widget(&widget) {}

  void update(const Flex<Children...>& newWidget) {
    if (newWidget.axis != widget->axis) {
      this->markNeedsLayout();
      this->markNeedsDraw();
    }

    std::apply(
      [this](const auto&... childWidget) {
        int i = 0;
        (childWidget.update(*this->children[i++]), ...);
      },
      newWidget.children);
    widget = &newWidget;
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    // assert(isVertical() ? constraints.hasBoundedHeight()
    //                     : constraints.hasBoundedWidth());

    Size result = { 0, 0 };

    const auto childConstraints =
      isVertical()
        ? Constraints{ { constraints.min.width, 0 },
                       { constraints.max.width, Constraints::unbound } }
        : Constraints{ { 0, constraints.min.height },
                       { Constraints::unbound, constraints.max.height } };

    for (auto i = 0u; i < num_children; i++) {
      const auto newSize = this->children[i]->layout(childConstraints);
      if (isVertical() ? newSize.height != childSizes[i].height
                       : newSize.width != childSizes[i].width) {
        this->markNeedsDraw();
      }

      childSizes[i] = newSize;

      if (isVertical()) {
        result.height += childSizes[i].height;
        result.width = std::max(childSizes[i].width, result.width);
      } else {
        result.width += childSizes[i].width;
        result.height = std::max(childSizes[i].height, result.height);
      }
    }

    assert(result.height <= constraints.max.height && "Flex too large");
    assert(result.width <= constraints.max.width && "Flex too wide");

    totalSize = isVertical() ? result.height : result.width;

    // Align on each axis:
    if (result.height < constraints.min.height) {
      result.height = constraints.min.height;
    }

    if (result.width < constraints.min.width) {
      result.width = constraints.min.width;
    }

    return result;
  }

  UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) override {
    UpdateRegion result;

    const auto maxSize = isVertical() ? rect.height() : rect.width();
    auto offset = (maxSize - totalSize) / 2;

    for (auto i = 0u; i < num_children; i++) {
      const auto& size = childSizes[i];
      const auto& child = this->children[i];

      const auto otherOffset = isVertical() ? (rect.width() - size.width) / 2
                                            : (rect.height() - size.height) / 2;
      const auto offsetPoint = isVertical()
                                 ? rmlib::Point{ otherOffset, offset }
                                 : rmlib::Point{ offset, otherOffset };

      const auto topLeft = rect.topLeft + offsetPoint;
      const auto bottomRight = topLeft + size.toPoint();
      const auto subRect = rmlib::Rect{ topLeft, bottomRight };

      if (i == 0) {
        result = child->draw(subRect, canvas);
      } else {
        result |= child->draw(subRect, canvas);
      }
      offset += isVertical() ? size.height : size.width;
    }

    return result;
  }

private:
  constexpr bool isVertical() const { return widget->axis == Axis::Vertical; }

  const Flex<Children...>* widget;
  constexpr static auto num_children = sizeof...(Children);

  // TODO: remove both:
  std::array<Size, num_children> childSizes;
  int totalSize;
};

template<typename... Children>
class Flex : public Widget<FlexRenderObject<Children...>> {
private:
public:
  Flex(Axis axis, Children... children)
    : children(std::move(children)...), axis(axis) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<FlexRenderObject<Children...>>(*this);
  }

private:
  friend class FlexRenderObject<Children...>;
  std::tuple<Children...> children;
  Axis axis;
};

template<typename... Children>
class Column : public Flex<Children...> {
public:
  Column(Children... children)
    : Flex<Children...>(Axis::Vertical, std::move(children)...) {}
};

template<typename... Children>
class Row : public Flex<Children...> {
public:
  Row(Children... children)
    : Flex<Children...>(Axis::Horizontal, std::move(children)...) {}
};

} // namespace rmlib
