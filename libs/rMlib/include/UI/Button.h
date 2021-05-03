#pragma once

#include <UI/Gesture.h>
#include <UI/StatefulWidget.h>

namespace rmlib {

class Button : public StatefulWidget<Button> {
  class State : public StateBase<Button> {
  public:
    auto build(AppContext& ctx, const BuildContext& buildCtx) const {
      return GestureDetector(Padding(Border(Border(Text(getWidget().text),
                                                   Insets::all(2),
                                                   down ? black : white),
                                            Insets::all(2)),
                                     Insets::all(2)),
                             Gestures{}
                               .OnTouchDown([this](auto pos) {
                                 setState([](auto& self) { self.down = true; });
                               })
                               .OnTap([this] {
                                 setState(
                                   [](auto& self) { self.down = false; });
                                 getWidget().onClick();
                               }));
    }

  private:
    bool down = false;
  };

public:
  Button(std::string text, Callback onClick)
    : text(std::move(text)), onClick(std::move(onClick)) {}

  State createState() const { return State{}; }

private:
  std::string text;
  Callback onClick;
};

} // namespace rmlib
