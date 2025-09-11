#pragma once

#include "KeyPad.h"
#include "Screen.h"

#include <UI.h>

#include <tilem.h>

namespace tilem {
class CalcState;

class Calculator : public rmlib::StatefulWidget<Calculator> {
public:
  Calculator(std::string romPath, bool fullScreen = false);

  static CalcState createState();

private:
  friend class CalcState;

  std::string romPath;
  std::string savePath;
  bool fullScreen;
};

class CalcState : public rmlib::StateBase<Calculator> {
public:
  void init(rmlib::AppContext& context, const rmlib::BuildContext& buildCtx);

  static auto closeButton(rmlib::AppContext& context, int fontSize) {
    using namespace rmlib;

    return Sized(GestureDetector(
                   Border(Text("X", fontSize), Insets{ 0, 0, /* left */ 2, 0 }),
                   Gestures{}.onTap([&context] { context.stop(); })),
                 fontSize,
                 fontSize);
  }

  rmlib::DynamicWidget header(rmlib::AppContext& context, int width) const {
    using namespace rmlib;

    constexpr auto font_size = 48;

    if (getWidget().fullScreen) {
      return Sized(Colored(white), width, std::nullopt);
    }

    // TODO: expand option
    return Border(
      Row(Sized(Text("TilEm", font_size), width - font_size - 2, std::nullopt),
          closeButton(context, font_size)),
      Insets::all(1));
  }

  auto build(rmlib::AppContext& context,
             const rmlib::BuildContext& buildCtx) const {
    using namespace rmlib;

    constexpr auto default_width = 96;
    constexpr auto default_height = 64;

    const auto scale = getWidget().fullScreen
                         ? double(context.getFbCanvas().width()) / default_width
                         : 6.5;
    const auto width = std::floor(scale * default_width);
    const auto height = std::floor(scale * default_height);

    return Cleared(
      Center(Border(Column(header(context, width),
                           Sized(Screen(mCalc), width, height),
                           Expanded(Sized(Keypad(mCalc), width, std::nullopt))),
                    Insets::all(1))));
  }

  ~CalcState();

private:
  TilemCalc* loadCalc() const;

  void updateCalcState();

  TilemCalc* mCalc = nullptr;

  rmlib::TimerHandle updateTimer;
  rmlib::TimerHandle popupTimer;

  std::chrono::steady_clock::time_point lastUpdateTime;
};
} // namespace tilem
