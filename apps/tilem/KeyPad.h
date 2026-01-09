#pragma once

#include <Input.h>
#include <UI.h>

#include "tilem.h"

namespace tilem {

struct Key;
class KeypadRenderObject;

class Keypad : public rmlib::Widget<KeypadRenderObject> {
public:
  Keypad(TilemCalc* calc);

  std::unique_ptr<rmlib::RenderObject> createRenderObject() const;

private:
  friend class KeypadRenderObject;
  TilemCalc* calc = nullptr;
  size_t maxRowSize;
  size_t numRows;
};

class KeypadRenderObject : public rmlib::LeafRenderObject<Keypad> {
public:
  using LeafRenderObject<Keypad>::LeafRenderObject;

  void update(const Keypad& newWidget) { widget = &newWidget; }

protected:
  rmlib::Size doLayout(const rmlib::Constraints& constraints) final;
  void doHandleInput(const rmlib::input::Event& ev) final;
  rmlib::UpdateRegion doDraw(rmlib::Canvas& canvas) final;

  void drawKey(rmlib::Canvas& canvas,
               rmlib::Point pos,
               const Key& key,
               int keyWidth) const;

private:
  constexpr static auto key_aspect = 1.5;
  constexpr static auto front_label_factor = 0.6;

  std::vector<std::pair<rmlib::Rect, const Key*>> keyLocations;
  std::unordered_map<int, const Key*> keyPointers;
  int keyWidth{};
  int keyHeight{};
  int padding{};
};
} // namespace tilem
