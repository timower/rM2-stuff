#include "Calculator.h"

#include <UI/Navigator.h>

using namespace rmlib;
using namespace rmlib::input;

int
main(int argc, char* argv[]) {
  const auto* home = getenv("HOME");
  const auto defaultRom = home == nullptr
                            ? std::string("/home/root/ti84plus.rom")
                            : std::string(home) + "/ti84plus.rom";
  const char* calcName = nullptr;
  bool fullScreen = false;

  for (int i = 1; i < argc; i++) {
    if (argv[i] == std::string_view("--full")) {
      fullScreen = true;
    } else if (calcName == nullptr) {
      calcName = argv[i];
    } else {
      std::cerr << "Too many arguments!\n";
      return EXIT_FAILURE;
    }
  }

  if (calcName == nullptr) {
    calcName = defaultRom.c_str();
  }

  unistdpp::fatalOnError(
    runApp(Center(Navigator(tilem::Calculator(calcName, fullScreen)))));

  return EXIT_SUCCESS;
}
