#include "Launcher.h"

#include <UI.h>

using namespace rmlib;

int
main(int argc, char* argv[]) {
  unistdpp::fatalOnError(runApp(LauncherWidget()));

  auto fb = fb::FrameBuffer::open();
  if (fb.has_value()) {
    fb->canvas.set(white);
    fb->doUpdate(
      fb->canvas.rect(), fb::Waveform::GC16Fast, fb::UpdateFlags::None);
  }

  return EXIT_SUCCESS;
}
