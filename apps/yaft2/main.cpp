// yaft(2)
#include "YaftWidget.h"
#include "config.h"

// rmLib
#include <UI.h>

// stdlib
#include <unistd.h>

using namespace rmlib;

namespace {
const char* shellCmd = "/bin/bash";
}

int
main(int argc, char* argv[]) {
  static const char* shellArgs[3] = { shellCmd, "-l", nullptr };

  /* for wcwidth() */
  char* locale = nullptr;
  if ((locale = setlocale(LC_ALL, "en_US.UTF-8")) == nullptr &&
      (locale = setlocale(LC_ALL, "")) == nullptr) {
    std::cout << "setlocale failed\n";
  }
  std::cout << "Locale is: " << locale << "\n";

  const char* cmd = nullptr;
  char* const* args = nullptr;
  if (argc > 1) {
    cmd = argv[1];
    args = argv + 1;
  } else {
    cmd = shellArgs[0];
    args = const_cast<char* const*>(shellArgs);
  }

  auto cfg = loadConfigOrMakeDefault();

  fatalOnError(runApp(Yaft(cmd, args, std::move(cfg))));

  return 0;
}
