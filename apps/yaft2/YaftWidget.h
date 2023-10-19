#pragma once

#include "config.h"
#include "keyboard.h"
#include "layout.h"
#include "screen.h"

#include <yaft.h>

#include <UI/StatefulWidget.h>

class YaftState;

class Yaft : public rmlib::StatefulWidget<Yaft> {
public:
  Yaft(const char* cmd, char* const argv[], YaftConfigAndError config)
    : config(std::move(config.config))
    , configError(std::move(config.err))
    , cmd(cmd)
    , argv(argv) {}

  YaftState createState() const;

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

  void init(rmlib::AppContext& ctx, const rmlib::BuildContext&);

  void checkLandscape(rmlib::AppContext& ctx);

  auto build(rmlib::AppContext& ctx,
             const rmlib::BuildContext& buildCtx) const {
    using namespace rmlib;

    const auto& layout = [this]() -> const Layout& {
      if (isLandscape) {
        return empty_layout;
      }

      if (hidden) {
        return hidden_layout;
      }

      return *getWidget().config.layout;
    }();

    return Column(
      Expanded(Screen(term.get(), isLandscape, getWidget().config.autoRefresh)),
      Keyboard(
        term.get(), { layout, *getWidget().config.keymap }, [this](int num) {
          setState([](auto& self) { self.hidden = !self.hidden; });
        }));
  }

private:
  std::unique_ptr<terminal_t> term;
  rmlib::TimerHandle pogoTimer;

  bool hidden = false;
  bool isLandscape = false;
};
