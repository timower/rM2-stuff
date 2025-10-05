#include "Launcher.h"

#include <UI.h>

using namespace rmlib;

int
main(int argc, char* argv[]) {
  unistdpp::fatalOnError(runApp(LauncherWidget(), {}, /*clearOnExit=*/true));
  return EXIT_SUCCESS;
}
