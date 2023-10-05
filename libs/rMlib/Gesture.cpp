#include "Input.h"

#include <iostream>

#if !defined(EMULATE) || defined(EMULATE_UINPUT)
#include <libevdev/libevdev.h>
#endif

namespace rmlib::input {

namespace {

SwipeGesture::Direction
getSwipeDirection(Point delta) {
  if (std::abs(delta.x) > std::abs(delta.y)) {
    return delta.x > 0 ? SwipeGesture::Right : SwipeGesture::Left;
  }

  return delta.y > 0 ? SwipeGesture::Down : SwipeGesture::Up;
}

PinchGesture::Direction
getPinchDirection() {
  // TODO
  return PinchGesture::In;
}

} // namespace

Gesture
GestureController::getGesture(Point currentDelta) {
  const auto [avgStart, delta] = [this] {
    std::vector<Point> delta;
    Point avgStart;
    int nActive = 0;

#ifndef NDEBUG
    std::cout << "- delta: ";
#endif

    for (const auto& slot : slots) {
      if (slot.active) {
        delta.push_back(slot.currentPos - slot.startPos);

#ifndef NDEBUG
        std::cout << delta.back() << " ";
#endif

        avgStart += slot.startPos;
        nActive++;
      }
    }

#ifndef NDEBUG
    std::cout << std::endl;
#endif

    avgStart /= nActive;
    return std::make_pair(avgStart, std::move(delta));
  }();

  const auto isSwipe =
    std::all_of(
      delta.cbegin(), delta.cend(), [](auto& p) { return p.x >= 0; }) ||
    std::all_of(
      delta.cbegin(), delta.cend(), [](auto& p) { return p.x <= 0; }) ||
    std::all_of(
      delta.cbegin(), delta.cend(), [](auto& p) { return p.y >= 0; }) ||
    std::all_of(delta.cbegin(), delta.cend(), [](auto& p) { return p.y <= 0; });

  if (isSwipe) {
    return SwipeGesture{ getSwipeDirection(currentDelta),
                         avgStart,
                         /* endPos */ {},
                         getCurrentFingers() };
  }

  return PinchGesture{ getPinchDirection(), avgStart, getCurrentFingers() };
}

void
GestureController::handleTouchDown(const TouchEvent& event) {
  auto& slot = slots.at(event.slot);
  slot.active = true;

  std::cerr << "Touch down, current fingers: " << getCurrentFingers()
            << std::endl;

  slot.currentPos = event.location;
  slot.startPos = event.location;
  slot.time = std::chrono::steady_clock::now();

  tapFingers = getCurrentFingers();
}

std::optional<Gesture>
GestureController::handleTouchUp(const TouchEvent& event) {
  std::optional<Gesture> result;

  auto& slot = slots.at(event.slot);
  slot.active = false;

  std::cerr << "Touch up, current fingers: " << getCurrentFingers()
            << std::endl;

  if (getCurrentFingers() == 0) {
    if (!started) {
      // TODO: do we need a time limit?
      // auto delta = std::chrono::steady_clock::now() -
      // slots[event.slot].time; if (delta < tap_time) {
      result = TapGesture{ tapFingers, slot.startPos };
      //}
    } else {
      if (std::holds_alternative<SwipeGesture>(gesture)) {
        std::get<SwipeGesture>(gesture).endPosition = event.location;
      }
      result = gesture;
    }

    reset();
  }

  return result;
}

void
GestureController::handleTouchMove(const TouchEvent& event) {
  auto& slot = slots.at(event.slot);
  slot.currentPos = event.location;
  auto delta = event.location - slot.startPos;

  if (!started) {
    if (getCurrentFingers() >= 2 && (std::abs(delta.x) >= start_threshold ||
                                     std::abs(delta.y) >= start_threshold)) {

      started = true;
      gesture = getGesture(delta);
    }

    return;
  }

  // Started, calc percentage (TODO)
}

std::pair<std::vector<Gesture>, std::vector<Event>>
GestureController::handleEvents(const std::vector<Event>& events) {
  std::vector<Gesture> result;
  std::vector<Event> unhandled;

  for (const auto& event : events) {
    if (!std::holds_alternative<TouchEvent>(event)) {
      unhandled.emplace_back(event);
      continue;
    }
    const auto& touchEv = std::get<TouchEvent>(event);
    switch (touchEv.type) {
      case TouchEvent::Down:
        handleTouchDown(touchEv);
        break;
      case TouchEvent::Move:
        handleTouchMove(touchEv);
        break;
      case TouchEvent::Up: {
        auto gesture = handleTouchUp(touchEv);
        if (gesture.has_value()) {
          result.emplace_back(*gesture);
        }
        break;
      }
    }
  }

  return { result, unhandled };
}

void
GestureController::sync(InputDeviceBase& device) {
#if !defined(EMULATE) || defined(EMULATE_UINPUT)
  const auto maxSlots =
    std::min<int>(int(slots.size()), libevdev_get_num_slots(device.evdev));
  for (int i = 0; i < maxSlots; i++) {
    auto id = libevdev_get_slot_value(device.evdev, i, ABS_MT_TRACKING_ID);
    bool active = id != -1;
    if (active != slots[i].active) {
      std::cerr << "Desync for slot " << i << std::endl;
      if (!active) {
        slots[i].active = false;
        if (getCurrentFingers() == 0) {
          reset();
        }
      }
    }
  }
#endif
}

} // namespace rmlib::input
