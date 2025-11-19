#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

namespace rmlib {
template<typename Child>
class Rotated;

template<typename Child>
class RotatedRenderObject : public SingleChildRenderObject<Rotated<Child>> {
public:
  using SingleChildRenderObject<Rotated<Child>>::SingleChildRenderObject;

  void update(const Rotated<Child>& newWidget) {
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

  Size doLayout(const Constraints& constraints) override {
    const auto rot = this->getWidget().rot;
    auto res = this->child->layout(rotate(invert(rot), constraints));
    return rotate(rot, res);
  }

  UpdateRegion doDraw(Canvas& canvas) override {
    const auto rot = this->getWidget().rot;
    auto subCanvas = canvas.subCanvas(canvas.rect(), invert(rot));
    auto res = this->child->draw(subCanvas, { 0, 0 });
    res.region = rotate(subCanvas.rect().size(), invert(rot), res.region);
    return res;
  }

  virtual void doHandleInput(const input::Event& inEv) {
    auto ev = inEv;
    std::visit(
      [this](auto& ev) {
        if constexpr (input::is_pointer_event<decltype(ev)>) {
          ev.location =
            rotate(this->getSize(), this->getWidget().rot, ev.location);
        }
      },
      ev);
    this->child->handleInput(ev);
  }
};

template<typename Child>
class Rotated : public Widget<RotatedRenderObject<Child>> {
public:
  Rotated(Rotation rot, Child child) : rot(rot), child(std::move(child)) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<RotatedRenderObject<Child>>(*this);
  }

  Rotation rot;
  Child child;
};

} // namespace rmlib
