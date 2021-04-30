#pragma once

#include <UI/Layout.h>
#include <UI/RenderObject.h>
#include <UI/Text.h>
#include <UI/Widget.h>

namespace rmlib {

using PosCallback = std::function<void(Point)>;
using KeyCallback = std::function<void(int)>;

struct Gestures {
  Callback onAny;
  Callback onTap;

  PosCallback onTouchMove;
  PosCallback onTouchDown;

  KeyCallback onKeyDown;
  KeyCallback onKeyUp;

  Gestures& OnTap(Callback cb) {
    onTap = std::move(cb);
    return *this;
  }

  Gestures& OnTouchMove(PosCallback cb) {
    onTouchMove = std::move(cb);
    return *this;
  }

  Gestures& OnTouchDown(PosCallback cb) {
    onTouchDown = std::move(cb);
    return *this;
  }

  Gestures& OnKeyDown(KeyCallback cb) {
    onKeyDown = std::move(cb);
    return *this;
  }

  Gestures& OnKeyUp(KeyCallback cb) {
    onKeyUp = std::move(cb);
    return *this;
  }

  Gestures& OnAny(Callback cb) {
    onAny = std::move(cb);
    return *this;
  }

  bool handlesTouch() const { return onTap || onTouchDown || onTouchMove; }
};

template<typename Child>
class GestureDetector;

template<typename Child>
class GestureRenderObject
  : public SingleChildRenderObject<GestureDetector<Child>> {
public:
  using SingleChildRenderObject<
    GestureDetector<Child>>::SingleChildRenderObject;
  // GestureRenderObject(const GestureDetector<Child>& widget)
  //   : SingleChildRenderObject(widget.child.createRenderObject())
  //   , widget(&widget) {}

  void handleInput(const rmlib::input::Event& ev) final {
    if (this->widget->gestures.onAny) {
      this->widget->gestures.onAny();
    }

    std::visit(
      [this](const auto& ev) {
        if constexpr (input::is_pointer_event<decltype(ev)>) {
          if (ev.isDown() && this->getRect().contains(ev.location) &&
              currentId == -1) {
            if (this->widget->gestures.handlesTouch()) {
              currentId = ev.id;
            }

            if (this->widget->gestures.onTouchDown) {
              this->widget->gestures.onTouchDown(ev.location);
              return;
            }
          }

          if (ev.id == currentId) {
            if (ev.isUp()) {
              currentId = -1;
              if (this->widget->gestures.onTap) {
                this->widget->gestures.onTap();
              }
              return;
            }

            if (ev.isMove() && this->widget->gestures.onTouchMove) {
              this->widget->gestures.onTouchMove(ev.location);
              return;
            }
          }
        } else {
          if (ev.type == input::KeyEvent::Press &&
              this->widget->gestures.onKeyDown) {
            this->widget->gestures.onKeyDown(ev.keyCode);
            return;
          }

          if (ev.type == input::KeyEvent::Release &&
              this->widget->gestures.onKeyUp) {
            this->widget->gestures.onKeyUp(ev.keyCode);
            return;
          }
        }

        // If we didn't return yet we didn't handle the event.
        // So let our child handle it.
        SingleChildRenderObject<GestureDetector<Child>>::handleInput(ev);
      },
      ev);
  }

  void update(const GestureDetector<Child>& newWidget) {
    this->widget = &newWidget;
    this->widget->child.update(*this->child);
  }

private:
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

auto
Button(std::string text, Callback onTap) {
  return GestureDetector(
    Container(
      Text(std::move(text)), Insets::all(2), Insets::all(2), Insets::all(1)),
    Gestures{}.OnTap(std::move(onTap)));
}
} // namespace rmlib
