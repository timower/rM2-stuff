#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

namespace rmlib {

template<typename Derived>
class StatelessWidget;

template<typename Derived>
class StatelessRenderObject : public SingleChildRenderObject<Derived> {
  using WidgetT = std::result_of_t<decltype (
    &Derived::build)(const Derived, AppContext&, const BuildContext&)>;

public:
  StatelessRenderObject(const Derived& widget)
    : SingleChildRenderObject<Derived>(widget, nullptr) {
    this->markNeedsRebuild();
  }

  void update(const Derived& newWidget) {
    this->widget = &newWidget;
    this->markNeedsRebuild();

    // Don't update the current widget as we need to rebuild anyways.
    // currentWidget()->update(*child);
  }

protected:
  void doRebuild(AppContext& context, const BuildContext& buildCtx) override {
    otherWidget().emplace(std::move(this->widget->build(context, buildCtx)));
    if (this->child == nullptr) {
      this->child = otherWidget()->createRenderObject();
    } else {
      otherWidget()->update(*this->child);
    }
    swapWidgets();
  }

private:
  std::optional<WidgetT>& currentWidget() { return buildWidgets[currentIdx]; }
  std::optional<WidgetT>& otherWidget() { return buildWidgets[1 - currentIdx]; }
  void swapWidgets() { currentIdx = 1 - currentIdx; }

  std::array<std::optional<WidgetT>, 2> buildWidgets;
  int currentIdx = 0;
};

template<typename Derived>
class StatelessWidget : public Widget<StatelessRenderObject<Derived>> {
public:
  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<StatelessRenderObject<Derived>>(
      *static_cast<const Derived*>(this));
  }

private:
  friend Derived;
  StatelessWidget() {}
};

} // namespace rmlib
