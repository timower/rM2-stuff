#include "Calculator.h"

#include <UI/Navigator.h>

using namespace rmlib;
using namespace rmlib::input;

namespace {

constexpr auto calc_default_rom = "/home/root/ti84plus.rom";

} // namespace

int
main(int argc, char* argv[]) {
  const auto* calc_name = argc > 1 ? argv[1] : calc_default_rom;

  const auto err = runApp(Navigator(tilem::Calculator(calc_name)));

  if (!err.has_value()) {
    std::cerr << err.error().msg << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
