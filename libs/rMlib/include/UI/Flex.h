#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

#include <numeric>

namespace rmlib {

// TODO: this won't work with DynamicWidgets..
// TODO: will this work with changing flex?
template<typename Child>
class Expanded {
public:
  Expanded(Child child, float flex = 1.0f)
    : child(std::move(child)), flex(flex) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return child.createRenderObject();
  }

  void update(RenderObject& ro) const { child.update(ro); }

  Child child;
  float flex;
};

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

    // First layout non flex children with unbounded contraints.
    for (auto i = 0u; i < num_children; i++) {
      if (this->widget->flexes[i] != 0) {
        continue;
      }

      // TODO: DRY
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

    // Divide the remaining space.
    const auto totalFlex = std::accumulate(
      this->widget->flexes.begin(), this->widget->flexes.end(), 0);
    const auto remainingSpace = isVertical()
                                  ? constraints.max.height - result.height
                                  : constraints.max.width - result.width;

    // TODO: if totalFlex or remainingSpace changed, then we need to re-layout

    for (auto i = 0u; i < num_children; i++) {
      if (this->widget->flexes[i] == 0) {
        continue;
      }

      const auto sizeAllocation = int(remainingSpace / totalFlex);
      const auto childConstraints =
        isVertical()
          ? Constraints{ { constraints.min.width, sizeAllocation },
                         { constraints.max.width, sizeAllocation } }
          : Constraints{ { sizeAllocation, constraints.min.height },
                         { sizeAllocation, constraints.max.height } };

      const auto newSize = this->children[i]->layout(childConstraints);

      // TODO: DRY
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
  template<typename T>
  struct IsExpanded : std::false_type {};

  template<typename T>
  struct IsExpanded<Expanded<T>> : std::true_type {};

  template<typename T>
  static float GetFlex(const T& t) {
    if constexpr (IsExpanded<T>::value) {
      return t.flex;
    } else {
      return 0;
    }
  }

public:
  Flex(Axis axis, Children... children)
    : flexes{ GetFlex(children)... }
    , children(std::move(children)...)
    , axis(axis) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<FlexRenderObject<Children...>>(*this);
  }

private:
  friend class FlexRenderObject<Children...>;

  std::array<float, sizeof...(Children)> flexes;
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
