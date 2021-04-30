#pragma once

#include <UI/DynamicWidget.h>
#include <UI/Future.h>
#include <UI/Stack.h>
#include <UI/StatefulWidget.h>

#include <any>

namespace rmlib {

namespace details {
class UniqueAny {
  struct AnyBase {
    AnyBase(typeID::type_id_t id) : currentType(id) {}

    typeID::type_id_t currentType;
  };

  template<typename T>
  struct AnyImpl : AnyBase {
    AnyImpl(T val) : AnyBase(typeID::type_id<T>()), value(std::move(val)) {}
    T value;
  };

public:
  UniqueAny() = default;

  template<typename T>
  UniqueAny(T value)
    : contents(std::make_unique<AnyImpl<T>>(std::move(value))) {}

  bool hasValue() const { return contents != nullptr; }

  template<typename T>
  T get() {
    assert(contents->currentType == typeID::type_id<T>());
    auto* impl = static_cast<AnyImpl<T>*>(contents.get());
    return std::move(impl->value);
  }

private:
  std::unique_ptr<AnyBase> contents;
};
} // namespace details

/// TODO: should this take the build context?
using WidgetBuilder = std::function<DynamicWidget()>;

struct OverlayEntry {
  WidgetBuilder builder;
  details::UniqueAny promise;
};

class Navigator : public StatefulWidget<Navigator> {
private:
  class State : public StateBase<Navigator> {
  public:
    void init(AppContext&, const BuildContext&) {
      entries.reserve(getWidget().initElems.size());
      std::transform(getWidget().initElems.begin(),
                     getWidget().initElems.end(),
                     std::back_inserter(entries),
                     [](auto& builder) {
                       return OverlayEntry{ builder, details::UniqueAny{} };
                     });
    }

    auto build(AppContext& context, const BuildContext&) const {
      std::vector<DynamicWidget> widgets;
      widgets.reserve(entries.size());
      std::transform(entries.begin(),
                     entries.end(),
                     std::back_inserter(widgets),
                     [](const auto& entry) { return entry.builder(); });
      return Stack(std::move(widgets));
    }

    Future<void> push(WidgetBuilder builder) const {
      auto promise = Promise<void>();
      auto future = promise.getFuture();
      auto anyPromise = details::UniqueAny(std::move(promise));

      auto entry = OverlayEntry{ std::move(builder), std::move(anyPromise) };
      modify().entries.push_back(std::move(entry));

      return future;
    }

    // TODO
    template<typename T>
    Future<T> push(WidgetBuilder builder) const {
      auto promise = Promise<T>();
      auto future = promise.getFuture();
      auto anyPromise = details::UniqueAny(std::move(promise));

      auto entry = OverlayEntry{ std::move(builder), std::move(anyPromise) };
      modify().entries.push_back(std::move(entry));

      return future;
    }

    template<typename T>
    void pop(T value) const {
      setState([value = std::move(value)](auto& self) {
        auto back = std::move(self.entries.back());
        self.entries.pop_back();

        if (back.promise.hasValue()) {
          auto promise = back.promise.template get<Promise<T>>();
          promise.setValue(std::move(value));
        }
      });
    }

    void pop() const {
      setState([](auto& self) {
        auto back = std::move(self.entries.back());
        self.entries.pop_back();

        if (back.promise.hasValue()) {
          auto promise = back.promise.template get<Promise<void>>();
          promise.setValue();
        }
      });
    }

  private:
    std::vector<OverlayEntry> entries;
  };

public:
  template<typename Widget>
  Navigator(Widget initWidget)
    : initElems(
        { [initWidget = std::move(initWidget)] { return initWidget; } }) {}

  Navigator(std::vector<WidgetBuilder> initElems)
    : initElems(std::move(initElems)) {}

  State createState() const { return State(); }

  static const State& of(const BuildContext& buildCtx) {
    return StatefulWidget::getState(buildCtx);
  }

private:
  std::vector<WidgetBuilder> initElems;
};

} // namespace rmlib
