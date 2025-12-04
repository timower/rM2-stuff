#pragma once

#include <UI.h>

template<typename Child>
class Hideable;

template<typename Child>
class HideableRenderObject
  : public rmlib::SingleChildRenderObject<Hideable<Child>> {

public:
  HideableRenderObject(const Hideable<Child>& widget)
    : rmlib::SingleChildRenderObject<Hideable<Child>>(
        widget,
        widget.child.has_value() ? widget.child->createRenderObject()
                                 : nullptr) {}

  rmlib::Size doLayout(const rmlib::Constraints& constraints) override {
    if (!this->widget->child.has_value()) {
      return constraints.min;
    }

    return this->child->layout(constraints);
  }

  rmlib::UpdateRegion cleanup(rmlib::Canvas& canvas) override {
    if (!this->widget->child.has_value()) {
      return {};
    }
    return rmlib::SingleChildRenderObject<Hideable<Child>>::cleanup(canvas);
  }

  void update(const Hideable<Child>& newWidget) {
    auto wasVisible = this->widget->child.has_value();
    this->widget = &newWidget;

    if (this->widget->child.has_value()) {
      if (this->child == nullptr) {
        this->child = this->widget->child->createRenderObject();
      } else {
        this->widget->child->update(*this->child);
      }

      if (!wasVisible) {
        doRefresh = true;
        this->markNeedsDraw();
        this->markNeedsLayout(); // TODO: why!!??
      }
    }
  }

  void doHandleInput(const rmlib::input::Event& ev) override {
    if (this->widget->child.has_value()) {
      this->child->handleInput(ev);
    }
  }

protected:
  rmlib::UpdateRegion doDraw(rmlib::Canvas& canvas) override {
    if (!this->widget->child.has_value()) {
      return rmlib::UpdateRegion{};
    }

    auto result = this->child->draw(canvas, { 0, 0 });
    if (doRefresh) {
      doRefresh = false;
      result.waveform = rmlib::fb::Waveform::GC16Fast;
      // result.flags = rmlib::fb::UpdateFlags::FullRefresh;
    }

    return result;
  }

private:
  rmlib::Size childSize{};
  bool doRefresh = false;
};

template<typename Child>
class Hideable : public rmlib::Widget<HideableRenderObject<Child>> {
public:
  Hideable(std::optional<Child> child) : child(std::move(child)) {}

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const {
    return std::make_unique<HideableRenderObject<Child>>(*this);
  }

private:
  friend class HideableRenderObject<Child>;
  std::optional<Child> child;
};
