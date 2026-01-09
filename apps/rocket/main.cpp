#include "Launcher.h"

#include <UI.h>

#ifdef __linux__
#include <systemd/sd-daemon.h>
#endif

using namespace rmlib;

int
main(int argc, char* argv[]) {
  if (getenv("ROCKET_WAIT_FOR_INPUT") != nullptr) {
    unistdpp::fatalOnError(fb::FrameBuffer::open());

    input::InputManager manager;
    unistdpp::fatalOnError(manager.openAll());
    while (manager.numDevices() == 0) {
      std::cerr << "Waiting for any device!\n";
      unistdpp::fatalOnError(manager.waitForInput({}));
    }
  }

#ifdef __linux__
  sd_notify(0, "READY=1");
#endif

  unistdpp::fatalOnError(runApp(LauncherWidget(), {}, /*clearOnExit=*/true));
  return EXIT_SUCCESS;
}
