#pragma once

#include <UI.h>

#include <tilem.h>

namespace tilem {

class ScreenRenderObject;

class Screen : public rmlib::Widget<ScreenRenderObject> {
public:
  Screen(TilemCalc* calc) : calc(calc) {}

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const;

private:
  friend class ScreenRenderObject;
  TilemCalc* calc = nullptr;
};

class ScreenRenderObject : public rmlib::LeafRenderObject<Screen> {
public:
  ScreenRenderObject(const Screen& widget);
  ~ScreenRenderObject();

  static void stateFrameCallback(TilemCalc* calc, void* selfPtr);

  void addTimer(TilemCalc* calc);

  void update(const Screen& newWidget);

protected:
  rmlib::Size doLayout(const rmlib::Constraints& constraints) final;

  rmlib::UpdateRegion doDraw(rmlib::Rect rect, rmlib::Canvas& canvas) final;

private:
  TilemLCDBuffer* lcd = nullptr;
  TilemLCDBuffer* oldLcd = nullptr;
  int timerID = -1;
};

} // namespace tilem
