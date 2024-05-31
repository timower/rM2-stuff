#pragma once

#include <UI.h>

#include "App.h"

const rmlib::MemoryCanvas&
getMissingImage();

class RunningAppWidget : public rmlib::StatelessWidget<RunningAppWidget> {
public:
  RunningAppWidget(const App& app,
                   rmlib::Callback onTap,
                   rmlib::Callback onKill,
                   bool isCurrent)
    : app(app)
    , onTap(std::move(onTap))
    , onKill(std::move(onKill))
    , isCurrent(isCurrent) {}

  auto build(rmlib::AppContext& /*unused*/,
             const rmlib::BuildContext& /*unused*/) const {
    using namespace rmlib;

    const Canvas& canvas = app.savedFB().has_value() ? app.savedFB()->canvas
                                                     : getMissingImage().canvas;

    return container(
      Column(GestureDetector(Sized(Image(canvas), 234, 300),
                             Gestures{}.onTap(onTap)),
             Row(Text(app.description().name), Button("X", onKill))),
      Insets::all(isCurrent ? 1 : 2),
      Insets::all(isCurrent ? 2 : 1),
      Insets::all(2));
  }

private:
  const App& app;
  rmlib::Callback onTap;
  rmlib::Callback onKill;
  bool isCurrent;
};

class AppWidget : public rmlib::StatelessWidget<AppWidget> {
public:
  AppWidget(const App& app, rmlib::Callback onLaunch)
    : app(app), onLaunch(std::move(onLaunch)) {}

  auto build(rmlib::AppContext& /*unused*/,
             const rmlib::BuildContext& /*unused*/) const {
    using namespace rmlib;

    const Canvas& canvas =
      app.icon().has_value() ? app.icon()->canvas : getMissingImage().canvas;
    return container(GestureDetector(Column(Sized(Image(canvas), 128, 128),
                                            Text(app.description().name)),
                                     Gestures{}.onTap(onLaunch)),
                     Insets::all(2),
                     Insets::all(1),
                     Insets::all(2));
  }

private:
  const App& app;
  rmlib::Callback onLaunch;
};
