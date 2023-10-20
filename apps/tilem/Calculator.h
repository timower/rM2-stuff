#pragma once

#include "KeyPad.h"
#include "Screen.h"

#include <UI.h>

#include <tilem.h>

namespace tilem {
class CalcState;

class Calculator : public rmlib::StatefulWidget<Calculator> {
public:
  Calculator(std::string romPath);

  CalcState createState() const;

private:
  friend class CalcState;

  std::string romPath;
  std::string savePath;
};

class CalcState : public rmlib::StateBase<Calculator> {
public:
  void init(rmlib::AppContext& context, const rmlib::BuildContext& buildCtx);

  auto closeButton(rmlib::AppContext& context, int fontSize) const {
    using namespace rmlib;

    return Sized(GestureDetector(
                   Border(Text("X", fontSize), Insets{ 0, 0, /* left */ 2, 0 }),
                   Gestures{}.OnTap([&context] { context.stop(); })),
                 fontSize,
                 fontSize);
  }

  auto header(rmlib::AppContext& context, int width) const {
    using namespace rmlib;

    constexpr auto fontSize = 48;
    // TODO: expand option
    return Border(
      Row(Sized(Text("TilEm", fontSize), width - fontSize - 2, std::nullopt),
          closeButton(context, fontSize)),
      Insets::all(1));
  }

  auto build(rmlib::AppContext& context,
             const rmlib::BuildContext& buildCtx) const {
    using namespace rmlib;

    constexpr auto scale = 6.5;
    constexpr auto width = scale * 96;
    constexpr auto height = scale * 64;

    return Cleared(
      Center(Border(Column(header(context, width),
                           Sized(Screen(mCalc), width, height),
                           Sized(Keypad(mCalc), width, std::nullopt)),
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
