#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

#include <memory>
#include <optional>

namespace rmlib {

template<typename Derived>
class StatefulRenderObject;

template<typename Derived>
class StatefulWidget;

template<typename DerivedSW>
class StateBase;

namespace details {

template<typename DerivedSW>
using StateType =
  std::result_of_t<decltype (&DerivedSW::createState)(const DerivedSW)>;

template<typename DerivedSW>
using WidgetType = std::result_of_t<decltype (&StateType<DerivedSW>::build)(
  const StateType<DerivedSW>,
  AppContext&,
  const BuildContext&)>;

} // namespace details

template<typename DerivedSW>
class StateBase {
public:
  void setRenderObject(StatefulRenderObject<DerivedSW>& renderObject) {
    mRenderObject = &renderObject;
  }

  void init(AppContext&, const BuildContext&) {
    static_assert(std::is_base_of_v<StateBase<DerivedSW>,
                                    typename details::StateType<DerivedSW>>,
                  "State must derive StateBase");
  }

protected:
  auto& modify() const {
    using StateT = typename details::StateType<DerivedSW>;

    mRenderObject->markNeedsRebuild();
    return *const_cast<StateT*>(static_cast<const StateT*>(this));
  }

  template<typename Fn>
  void setState(Fn fn) const {
    using StateT = typename details::StateType<DerivedSW>;

    fn(*const_cast<StateT*>(static_cast<const StateT*>(this)));
    mRenderObject->markNeedsRebuild();
  }

  const DerivedSW& getWidget() const { return mRenderObject->getWidget(); }

private:
  StatefulRenderObject<DerivedSW>* mRenderObject;
};

template<typename Derived>
class StatefulRenderObject : public SingleChildRenderObject<Derived> {
private:
  using StateT = typename details::StateType<Derived>;
  using WidgetT = typename details::WidgetType<Derived>;

public:
  StatefulRenderObject(const Derived& widget)
    : SingleChildRenderObject<Derived>(widget, nullptr)
    , state(widget.createState()) {
    state.setRenderObject(*this);
    this->markNeedsRebuild();
  }

  void update(const Derived& newWidget) {
    // currentWidget()->update(*child);

    this->widget = &newWidget;
    this->markNeedsRebuild();
  }

  const Derived& getWidget() const { return *this->widget; }
  const StateT& getState() const { return state; }

  // Provide a desctructor that destroys the child before destroying the
  // widgets.
  virtual ~StatefulRenderObject() { this->child = nullptr; }

protected:
  void doRebuild(AppContext& context, const BuildContext& buildCtx) override {
    if (!hasInitedState) {
      state.init(context, buildCtx);
      hasInitedState = true;
    }

    otherWidget().emplace(std::move(constState().build(context, buildCtx)));
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

  const StateT& constState() const { return state; }

  std::array<std::optional<WidgetT>, 2> buildWidgets;
  StateT state;

  int currentIdx = 0;

  bool hasInitedState = false;
};

template<typename Derived>
class StatefulWidget : public Widget<StatefulRenderObject<Derived>> {
public:
  std::unique_ptr<RenderObject> createRenderObject() const {
    static_assert(std::is_base_of_v<StatefulWidget<Derived>, Derived>,
                  "The widget must derive StatefulWidget");
    return std::make_unique<StatefulRenderObject<Derived>>(
      *static_cast<const Derived*>(this));
  }

  static const auto& getState(const BuildContext& buildCtx) {
    const auto* ro = buildCtx.getRenderObject<StatefulRenderObject<Derived>>();
    assert(ro != nullptr && "No such parent");
    return ro->getState();
  }

private:
  friend Derived;
  StatefulWidget() {}
};

} // namespace rmlib
