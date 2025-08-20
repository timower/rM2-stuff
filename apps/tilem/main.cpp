#include "Calculator.h"

#include <UI/Navigator.h>

using namespace rmlib;
using namespace rmlib::input;

namespace {

constexpr auto calc_default_rom = "/home/root/ti84plus.rom";

} // namespace

int
main(int argc, char* argv[]) {
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
    calcName = calc_default_rom;
  }

  unistdpp::fatalOnError(
    runApp(Navigator(tilem::Calculator(calcName, fullScreen))));

  return EXIT_SUCCESS;
}
