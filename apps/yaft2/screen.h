#pragma once

#include "terminal.h"
#include "yaft.h"

#include <UI.h>

class ScreenRenderObject;

class Screen : public rmlib::Widget<ScreenRenderObject> {
public:
  Screen(struct terminal_t* term, bool isLandscape, int autoRefresh)
    : term(term), isLandscape(isLandscape) {}

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const;

private:
  friend class ScreenRenderObject;
  struct terminal_t* term;

  bool isLandscape = false;
  int autoRefresh = 0;
};

class ScreenRenderObject : public rmlib::LeafRenderObject<Screen> {
public:
  ScreenRenderObject(const Screen& screen)
    : rmlib::LeafRenderObject<Screen>(screen) {
    markNeedsRebuild();
  }

  void update(const Screen& newWidget);

protected:
  rmlib::Size doLayout(const rmlib::Constraints& constraints) final;

  rmlib::UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) final;

  void doRebuild(rmlib::AppContext& ctx, const rmlib::BuildContext&) final;

  void handleInput(const rmlib::input::Event& ev) final;

private:
  rmlib::Rect drawLine(rmlib::Canvas& canvas,
                       rmlib::Rect rect,
                       terminal_t& term,
                       int line) const;
  template<typename Ev>
  void handleTouchEvent(const Ev& ev);

  bool shouldRefresh() const;

  int numUpdates = 0;

  int mouseSlot = -1;
  rmlib::Point lastMousePos;

  const rmlib::fb::FrameBuffer* fb = nullptr;
};
