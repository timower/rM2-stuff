#include <Device.h>
#include <Input.h>

#include <iostream>

using namespace rmlib;
using namespace rmlib::input;

template<typename T>
void
printType(T type) {}

void
printEvent(const TouchEvent& ev) {
  std::cout << "Touch ";
  switch (ev.type) {
    case TouchEvent::Down:
      std::cout << "Down";
      break;
    case TouchEvent::Up:
      std::cout << "Up";
      break;
    case TouchEvent::Move:
      std::cout << "Move";
      break;
  }
  std::cout << " at " << ev.location.x << "x" << ev.location.y;
  std::cout << " id " << ev.id << " slot " << ev.slot << std::endl;
}

void
printEvent(const PenEvent& ev) {
  std::cout << "Pen ";
  switch (ev.type) {
    case PenEvent::ToolClose:
      std::cout << "ToolClose";
      break;
    case PenEvent::ToolLeave:
      std::cout << "ToolLeave";
      break;
    case PenEvent::TouchDown:
      std::cout << "TouchDown";
      break;
    case PenEvent::TouchUp:
      std::cout << "TouchUp";
      break;
    case PenEvent::Move:
      std::cout << "Move";
      break;
  }
  std::cout << " at " << ev.location.x << "x" << ev.location.y;
  std::cout << " dist " << ev.distance << " pres " << ev.pressure << std::endl;
}

void
printEvent(const KeyEvent& ev) {
  std::cout << "Key ";
  switch (ev.type) {
    case KeyEvent::Press:
      std::cout << "Press ";
      break;
    case KeyEvent::Release:
      std::cout << "Release ";
      break;
    case KeyEvent::Repeat:
      std::cout << "Repeat ";
      break;
  }
  std::cout << ev.keyCode << std::endl;
}

int
main() {
  auto deviceType = device::getDeviceType();
  if (deviceType.isError()) {
    std::cerr << "Unknown device\n";
    return -1;
  }

  InputManager input;
  auto err = input.openAll();
  if (err.isError()) {
    std::cerr << err.getError().msg << std::endl;
  }

  while (true) {
    auto events = input.waitForInput(std::nullopt);
    if (!events.has_value()) {
      std::cerr << "timout or error\n";
    }

    for (auto& event : *events) {
      std::visit([](auto& e) { printEvent(e); }, event);
    }
  }

  return 0;
}
