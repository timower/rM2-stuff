#pragma once

#include "terminal.h"
#include "yaft.h"

#include <UI.h>

class ScreenRenderObject;

class Screen : public rmlib::Widget<ScreenRenderObject> {
public:
  Screen(struct terminal_t* term) : term(term) {}

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const;

private:
  friend class ScreenRenderObject;
  struct terminal_t* term;
};

class ScreenRenderObject : public rmlib::LeafRenderObject<Screen> {
public:
  using LeafRenderObject<Screen>::LeafRenderObject;

  void update(const Screen& newWidget);

protected:
  rmlib::Size doLayout(const rmlib::Constraints& constraints) final;

  rmlib::UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) final;

  void handleInput(const rmlib::input::Event& ev) final;

private:
  rmlib::UpdateRegion drawLine(rmlib::Canvas& canvas,
                               rmlib::Rect rect,
                               terminal_t& term,
                               int line) const;
  template<typename Ev>
  void handleTouchEvent(const Ev& ev);

  int mouseSlot = -1;
  rmlib::Point lastMousePos;
};
