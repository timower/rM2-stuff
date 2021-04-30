#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

#include <UI/DynamicWidget.h>
#include <UI/StatefulWidget.h>
#include <UI/StatelessWidget.h>

#include <UI/Flex.h>
#include <UI/Wrap.h>

#include <UI/Gesture.h>
#include <UI/Image.h>
#include <UI/Layout.h>
#include <UI/Text.h>

#include <UI/AppContext.h>
#include <UI/Util.h>

#include <csignal>

/// Ideas: (most stolen from flutter)
// * Widgets are cheap to create, so have no real state.
// * StatefulWidget has state in seperate object, making it still cheap
// * The state is actually associated with the underlying render object in the
//    scene tree.
namespace rmlib {

namespace details {
static AppContext* currentContext = nullptr;

void
stop(int signal) {
  currentContext->stop();
}

} // namespace details

template<typename AppWidget>
OptError<>
runApp(AppWidget widget) {
  auto fb = TRY(rmlib::fb::FrameBuffer::open());

  AppContext context(fb.canvas);
  details::currentContext = &context;

  TRY(context.getInputManager().openAll());

  auto rootRO = widget.createRenderObject();

  const auto fbSize = Size{ fb.canvas.width(), fb.canvas.height() };
  const auto constraints = Constraints{ fbSize, fbSize };

  std::signal(SIGINT, details::stop);
  std::signal(SIGTERM, details::stop);

  while (!context.shouldStop()) {
    rootRO->rebuild(context, nullptr);

    const auto size = rootRO->layout(constraints);
    const auto rect = rmlib::Rect{ { 0, 0 }, size.toPoint() };

    auto updateRegion = rootRO->cleanup(fb.canvas);
    updateRegion |= rootRO->draw(rect, fb.canvas);

    if (!updateRegion.region.empty()) {
      fb.doUpdate(
        updateRegion.region, updateRegion.waveform, updateRegion.flags);
    }

    const auto duration = context.getNextDuration();
    const auto evsOrError = context.getInputManager().waitForInput(duration);
    context.checkTimers();
    context.doAllLaters();

    if (evsOrError.isError()) {
      std::cerr << evsOrError.getError().msg << std::endl;
    } else {
      for (const auto& ev : *evsOrError) {
        rootRO->handleInput(ev);
      }
    }

    rootRO->reset();
  }

  std::signal(SIGINT, SIG_DFL);
  std::signal(SIGTERM, SIG_DFL);
  details::currentContext = nullptr;

  return NoError{};
}

} // namespace rmlib
