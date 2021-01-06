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
  if (!deviceType.has_value()) {
    std::cerr << "Unknown device\n";
    return -1;
  }

  auto inputs = device::getInputPaths(*deviceType);
  InputManager input;
  if (!input.open(inputs.touchPath.data(), inputs.touchTransform)) {
    std::cerr << "Error opening touch\n";
    return -1;
  }
  if (!input.open(inputs.penPath.data(), inputs.penTransform)) {
    std::cerr << "Error opening pen\n";
    return -1;
  }
  if (!input.open(inputs.buttonPath.data())) {
    std::cerr << "Error opening buttons\n";
    return -1;
  }

  while (true) {
    auto events = input.waitForInput();
    if (!events.has_value()) {
      std::cerr << "timout or error\n";
    }

    for (auto& event : *events) {
      std::visit([](auto& e) { printEvent(e); }, event);
    }
  }

  return 0;
}
