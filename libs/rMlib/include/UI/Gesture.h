#pragma once

#include <UI/Layout.h>
#include <UI/RenderObject.h>
#include <UI/Text.h>
#include <UI/Widget.h>

namespace rmlib {

using PosCallback = std::function<void(Point)>;
using KeyCallback = std::function<void(int)>;

struct Gestures {
  Callback onAnyFn;
  Callback onTapFn;

  PosCallback onTouchMoveFn;
  PosCallback onTouchDownFn;

  KeyCallback onKeyDownFn;
  KeyCallback onKeyUpFn;

  Gestures& onTap(Callback cb) {
    onTapFn = std::move(cb);
    return *this;
  }

  Gestures& onTouchMove(PosCallback cb) {
    onTouchMoveFn = std::move(cb);
    return *this;
  }

  Gestures& onTouchDown(PosCallback cb) {
    onTouchDownFn = std::move(cb);
    return *this;
  }

  Gestures& onKeyDown(KeyCallback cb) {
    onKeyDownFn = std::move(cb);
    return *this;
  }

  Gestures& onKeyUp(KeyCallback cb) {
    onKeyUpFn = std::move(cb);
    return *this;
  }

  Gestures& onAny(Callback cb) {
    onAnyFn = std::move(cb);
    return *this;
  }

  bool handlesTouch() const {
    return onTapFn || onTouchDownFn || onTouchMoveFn;
  }
};

template<typename Child>
class GestureDetector;

template<typename Child>
class GestureRenderObject
  : public SingleChildRenderObject<GestureDetector<Child>> {
public:
  using SingleChildRenderObject<
    GestureDetector<Child>>::SingleChildRenderObject;

  void doHandleInput(const rmlib::input::Event& ev) final {
    if (this->widget->gestures.onAnyFn) {
      this->widget->gestures.onAnyFn();
    }

    std::visit(
      [this](const auto& ev) {
        bool handled = false;
        if constexpr (input::is_pointer_event<decltype(ev)>) {
          handled = handlePointerEv(ev);
        } else {
          handled = handleKeyEv(ev);
        }

        if (handled) {
          return;
        }

        // If we didn't return yet we didn't handle the event.
        // So let our child handle it.
        SingleChildRenderObject<GestureDetector<Child>>::doHandleInput(ev);
      },
      ev);
  }

  void update(const GestureDetector<Child>& newWidget) {
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

private:
  template<typename Ev>
  bool handlePointerEv(const Ev& ev) {
    if (ev.isDown() && this->getLocalRect().contains(ev.location) &&
        currentId == -1) {
      if (this->widget->gestures.handlesTouch()) {
        currentId = ev.id;
      }

      if (this->widget->gestures.onTouchDownFn) {
        this->widget->gestures.onTouchDownFn(ev.location);
        return true;
      }
    }

    if (ev.id == currentId) {
      if (ev.isUp()) {
        currentId = -1;
        if (this->widget->gestures.onTapFn) {
          this->widget->gestures.onTapFn();
        }
        return true;
      }

      if (ev.isMove() && this->widget->gestures.onTouchMoveFn) {
        this->widget->gestures.onTouchMoveFn(ev.location);
        return true;
      }
    }
    return false;
  }

  bool handleKeyEv(const input::KeyEvent& ev) {
    if (ev.type == input::KeyEvent::Press &&
        this->widget->gestures.onKeyDownFn) {
      this->widget->gestures.onKeyDownFn(ev.keyCode);
      return true;
    }

    if (ev.type == input::KeyEvent::Release &&
        this->widget->gestures.onKeyUpFn) {
      this->widget->gestures.onKeyUpFn(ev.keyCode);
      return true;
    }
    return false;
  }

  int currentId = -1;
};

template<typename Child>
class GestureDetector : public Widget<GestureRenderObject<Child>> {
public:
  GestureDetector(Child child, Gestures gestures)
    : child(std::move(child)), gestures(std::move(gestures)) {}

  std::unique_ptr<RenderObject> createRenderObject() const {
    return std::make_unique<GestureRenderObject<Child>>(*this);
  }

  Child child;
  Gestures gestures;
};

} // namespace rmlib
