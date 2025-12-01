#pragma once

#include <rm2fb/ControlSocket.h>

#include <UI.h>
#include <UI/Rotate.h>

#include "App.h"

const rmlib::MemoryCanvas&
getMissingImage();

class RunningAppWidget : public rmlib::StatelessWidget<RunningAppWidget> {
public:
  RunningAppWidget(const ControlClient::Client& client,
                   rmlib::Callback onTap,
                   rmlib::Callback onKill,
                   bool isCurrent,
                   rmlib::Rotation rotation)
    : client(client)
    , onTap(std::move(onTap))
    , onKill(std::move(onKill))
    , rotation(rotation)
    , isCurrent(isCurrent) {}

  auto build(rmlib::AppContext& /*unused*/,
             const rmlib::BuildContext& /*unused*/) const {
    using namespace rmlib;

    const Canvas& canvas = /*app.savedFB().has_value() ? app.savedFB()->canvas
                                                     :*/
      getMissingImage().canvas;

    return container(
      Column(GestureDetector(Rotated(rotation, Sized(Image(canvas), 234, 300)),
                             Gestures{}.onTap(onTap)),
             Row(Text(client.name), Button("X", onKill))),
      Insets::all(isCurrent ? 1 : 2),
      Insets::all(isCurrent ? 2 : 1),
      Insets::all(2));
  }

private:
  const ControlClient::Client& client;
  rmlib::Callback onTap;
  rmlib::Callback onKill;
  rmlib::Rotation rotation;
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
      app.icon().has_value() ? *app.icon() : getMissingImage().canvas;
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
