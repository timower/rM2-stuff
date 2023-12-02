#include "Calculator.h"

#include <UI/Navigator.h>

using namespace rmlib;
using namespace rmlib::input;

namespace {

constexpr auto calc_default_rom = "/home/root/ti84plus.rom";

} // namespace

int
main(int argc, char* argv[]) {
  const auto* calcName = argc > 1 ? argv[1] : calc_default_rom;

  unistdpp::fatalOnError(runApp(Navigator(tilem::Calculator(calcName))));

  return EXIT_SUCCESS;
}
