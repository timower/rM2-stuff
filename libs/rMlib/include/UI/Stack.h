#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

#include <vector>

namespace rmlib {

template<typename Child>
class Stack;

template<typename Child>
class StackRenderObject : public MultiChildRenderObject<Stack<Child>> {
public:
  StackRenderObject(const Stack<Child>& widget)
    : MultiChildRenderObject<Stack<Child>>(getChildren(widget.children))
    , widget(&widget) {}

  void update(const Stack<Child>& newWidget) {
    bool needsDraw = newWidget.children.size() < this->children.size();
    this->updateChildren(*widget, newWidget);

    if (needsDraw) {
      this->markNeedsDraw();
    }

    widget = &newWidget;
  }

  void handleInput(const rmlib::input::Event& ev) override {
    if (!widget->onlyTopInput) {
      for (const auto& child : this->children) {
        child->handleInput(ev);
      }
      return;
    }

    if (this->children.empty()) {
      return;
    }

    this->children.back()->handleInput(ev);
  }

protected:
  Size doLayout(const Constraints& constraints) override {
    Size result = constraints.min;
    for (const auto& child : this->children) {
      Size childSize = child->layout(constraints);
      result.width = std::max(result.width, childSize.width);
      result.height = std::max(result.height, childSize.height);
    }

    return result;
  }

  UpdateRegion doDraw(Rect rect, Canvas& canvas) override {
    UpdateRegion result;

    for (const auto& child : this->children) {
      const auto subRect = rect.align(child->getSize(), 0, 0);
      result |= child->draw(subRect, canvas);
    }

    return result;
  }

private:
  const Stack<Child>* widget;
};

template<typename Child>
class Stack : public Widget<StackRenderObject<Child>> {
public:
  Stack(std::vector<Child> children, bool onlyTopInput = true)
    : children(std::move(children)), onlyTopInput(onlyTopInput) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<StackRenderObject<Child>>(*this);
  }

private:
  friend class StackRenderObject<Child>;
  friend class MultiChildRenderObject<Stack<Child>>;
  std::vector<Child> children;
  bool onlyTopInput;
};

} // namespace rmlib
