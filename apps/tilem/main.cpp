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
  const auto* calcName = argc > 1 ? argv[1] : defaultRom.c_str();

  unistdpp::fatalOnError(runApp(Navigator(tilem::Calculator(calcName))));

  return EXIT_SUCCESS;
}
