#pragma once

#include <UI/RenderObject.h>
#include <UI/Widget.h>

#include <UI/DynamicWidget.h>
#include <UI/StatefulWidget.h>
#include <UI/StatelessWidget.h>

#include <UI/Flex.h>
#include <UI/Wrap.h>

#include <UI/Button.h>
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
static inline AppContext* currentContext = nullptr; // NOLINT

inline void
stop(int signal) {
  currentContext->stop();
}

} // namespace details

template<typename AppWidget>
OptError<>
runApp(AppWidget widget,
       std::optional<Size> size = {},
       bool clearOnExit = false) {
  auto context = TRY(AppContext::makeContext(size));
  details::currentContext = &context;

  // TODO: fix widget lifetime
  context.setRootRenderObject(widget.createRenderObject());

  std::signal(SIGINT, details::stop);
  std::signal(SIGTERM, details::stop);

  while (!context.shouldStop()) {
    context.step();
  }

  std::signal(SIGINT, SIG_DFL);
  std::signal(SIGTERM, SIG_DFL);
  details::currentContext = nullptr;

  if (clearOnExit) {
    context.getFramebuffer().clear();
  }

  return {};
}

} // namespace rmlib
