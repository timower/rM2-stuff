#pragma once

#include <UI.h>

/// Widget that mainly blocks input when visible is false.
/// Still draws even when hidden, to ensure the current state (no splash) shows
/// up on resume.
template<typename Child>
class Hideable;

template<typename Child>
class HideableRenderObject
  : public rmlib::SingleChildRenderObject<Hideable<Child>> {

public:
  HideableRenderObject(const Hideable<Child>& widget)
    : rmlib::SingleChildRenderObject<Hideable<Child>>(
        widget,
        widget.child.createRenderObject()) {}

  rmlib::Size doLayout(const rmlib::Constraints& constraints) override {
    return this->child->layout(constraints);
  }

  rmlib::UpdateRegion cleanup(rmlib::Canvas& canvas) override {
    return rmlib::SingleChildRenderObject<Hideable<Child>>::cleanup(canvas);
  }

  void update(const Hideable<Child>& newWidget) {
    this->widget = &newWidget;
    this->widget->child.update(*this->child);

    if (this->widget->forceRefresh) {
      doFullRefresh = true;
      this->markNeedsDraw();
    }
  }

  void doHandleInput(const rmlib::input::Event& ev) override {
    if (this->widget->visible) {
      this->child->handleInput(ev);
    }
  }

protected:
  rmlib::UpdateRegion doDraw(rmlib::Canvas& canvas) override {
    auto result = this->child->draw(canvas, { 0, 0 });
    if (doFullRefresh) {
      doFullRefresh = false;
      result.waveform = rmlib::fb::Waveform::GC16Fast;
      result.flags = rmlib::fb::UpdateFlags::Sync;
    }
    return result;
  }

private:
  rmlib::Size childSize{};
  bool doFullRefresh = false;
};

template<typename Child>
class Hideable : public rmlib::Widget<HideableRenderObject<Child>> {
public:
  Hideable(Child child, bool visible, bool forceRefresh = false)
    : child(std::move(child)), visible(visible), forceRefresh(forceRefresh) {}

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const {
    return std::make_unique<HideableRenderObject<Child>>(*this);
  }

private:
  friend class HideableRenderObject<Child>;
  Child child;
  bool visible;
  bool forceRefresh;
};
