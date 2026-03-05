#include <Device.h>
#include <Input.h>

#include <csignal>
#include <iostream>

#include <sys/wait.h>
#include <unistd.h>

using namespace rmlib;
using namespace rmlib::input;

namespace {

volatile sig_atomic_t running = 1;
pid_t childPid = 0;

void
sigHandler(int /*sig*/) {
  running = 0;
}

void
reapChildren() {
  if (childPid != 0) {
    int status;
    pid_t result = waitpid(childPid, &status, WNOHANG);
    if (result == childPid) {
      childPid = 0;
    }
  }
}

void
launchTerminal() {
  if (childPid != 0) {
    return; // already running
  }

  pid_t pid = fork();
  if (pid == 0) {
    execl("/opt/bin/terminal", "terminal", nullptr);
    _exit(127);
  } else if (pid > 0) {
    childPid = pid;
  }
}

} // namespace

int
main() {
  signal(SIGINT, sigHandler);
  signal(SIGTERM, sigHandler);

  auto deviceType = device::getDeviceType();
  if (!deviceType.has_value()) {
    std::cerr << "keyhotd: unknown device\n";
    return 1;
  }

  InputManager input;
  auto err = input.openAll();
  if (!err.has_value()) {
    std::cerr << "keyhotd: " << err.error().msg << "\n";
    return 1;
  }

  std::cerr << "keyhotd: listening for Ctrl+Alt+T\n";

  bool ctrlHeld = false;
  bool altHeld = false;

  while (running) {
    reapChildren();

    auto events = input.waitForInput(std::chrono::milliseconds(1000));
    if (!events.has_value()) {
      continue;
    }

    for (auto& event : *events) {
      auto* key = std::get_if<KeyEvent>(&event);
      if (key == nullptr) {
        continue;
      }

      bool pressed = (key->type == KeyEvent::Press || key->type == KeyEvent::Repeat);
      bool released = (key->type == KeyEvent::Release);

      switch (key->keyCode) {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
          if (pressed) ctrlHeld = true;
          if (released) ctrlHeld = false;
          break;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
          if (pressed) altHeld = true;
          if (released) altHeld = false;
          break;
        case KEY_T:
          if (pressed && ctrlHeld && altHeld) {
            std::cerr << "keyhotd: Ctrl+Alt+T detected, launching terminal\n";
            launchTerminal();
          }
          break;
      }
    }
  }

  // Wait for child if running
  if (childPid != 0) {
    waitpid(childPid, nullptr, 0);
  }

  return 0;
}
