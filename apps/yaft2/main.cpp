// yaft(2)
#include "YaftWidget.h"
#include "config.h"

// rmLib
#include <UI.h>

// stdlib
#include <unistd.h>

using namespace rmlib;

namespace {
const char* shell_cmd = "/bin/bash";
}

int
main(int argc, char* argv[]) {
  static const char* shell_args[3] = { shell_cmd, "-l", NULL };

  if (setlocale(LC_ALL, "") == NULL) /* for wcwidth() */ {
    std::cout << "setlocale failed\n";
  }

  const char* cmd;
  char* const* args;
  if (argc > 1) {
    cmd = argv[1];
    args = argv + 1;
  } else {
    cmd = shell_args[0];
    args = const_cast<char* const*>(shell_args);
  }

  auto cfg = loadConfigOrMakeDefault();

  runApp(Yaft(cmd, args, std::move(cfg)));
  return 0;
}
