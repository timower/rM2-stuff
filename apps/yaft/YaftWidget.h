#pragma once

#include "config.h"
#include "keyboard.h"
#include "layout.h"
#include "screen.h"

#include <yaft.h>

#include <UI/Rotate.h>
#include <UI/StatefulWidget.h>

class YaftState;

class Yaft : public rmlib::StatefulWidget<Yaft> {
public:
  Yaft(const char* cmd, char* const argv[], YaftConfigAndError config)
    : config(std::move(config.config))
    , configError(std::move(config.err))
    , cmd(cmd)
    , argv(argv) {}

  static YaftState createState();

private:
  friend class YaftState;

  YaftConfig config;
  std::optional<YaftConfigError> configError;

  const char* cmd;
  char* const* argv;
};

class YaftState : public rmlib::StateBase<Yaft> {
public:
  ~YaftState();

  /// Logs the given string to the terminal console.
  void logTerm(std::string_view str);

  void init(rmlib::AppContext& ctx, const rmlib::BuildContext& /*unused*/);

  void checkLandscape(rmlib::AppContext& ctx);

  auto build(rmlib::AppContext& ctx,
             const rmlib::BuildContext& buildCtx) const {
    using namespace rmlib;

    const auto& cfg = getWidget().config;

    const auto& layout = [this, &cfg]() -> const Layout& {
      if (hideKeyboard) {
        return empty_layout;
      }

      if (smallKeyboard) {
        return hidden_layout;
      }

      return *cfg.layout;
    }();

    const auto repeatRateMs = std::chrono::milliseconds(1000) / cfg.repeatRate;
    return Rotated(rotation,
                   Column(Expanded(Screen(term.get(), false, cfg.autoRefresh)),
                          Keyboard(term.get(),
                                   KeyboardParams{
                                     .layout = layout,
                                     .keymap = *cfg.keymap,
                                     .repeatDelay = std::chrono::milliseconds(
                                       cfg.repeatDelay),
                                     .repeatTime = repeatRateMs,
                                   },
                                   [this](int num) {
                                     setState([](auto& self) {
                                       self.smallKeyboard = !self.smallKeyboard;
                                     });
                                   })));
  }

private:
  std::unique_ptr<terminal_t> term;
  rmlib::TimerHandle pogoTimer;

  rmlib::Rotation rotation = rmlib::Rotation::None;
  bool smallKeyboard = false;
  bool hideKeyboard = false;
};
