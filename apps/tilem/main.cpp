#include <FrameBuffer.h>
#include <Input.h>

#include "scancodes.h"
#include "tilem.h"

#include <atomic>
#include <csignal>
#include <iostream>
#include <optional>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "skin.h"

using namespace rmlib;
using namespace rmlib::input;

namespace {

volatile std::atomic_bool shouldStop = false;

std::optional<rmlib::ImageCanvas> skin;

std::optional<rmlib::Rect> lcd_rect;

TilemCalc* calc = nullptr;

constexpr auto calc_save_extension = ".sav";
constexpr auto calc_default_rom = "ti84p.rom";

bool
showCalculator(rmlib::fb::FrameBuffer& fb) {
  if (!skin.has_value()) {
    skin = rmlib::ImageCanvas::load(assets_skin_png, assets_skin_png_len);
    if (!skin.has_value()) {
      std::cerr << "Error loading image\n";
      return false;
    }
  }

  std::cout << "calc skin size: " << skin->canvas.width << "x"
            << skin->canvas.height << std::endl;

  rmlib::transform(
    fb.canvas,
    { 0, 0 },
    skin->canvas,
    skin->canvas.rect(),
    [](int x, int y, int val) { return ((val & 0xff) / 16) << 1; });

  fb.doUpdate(skin->canvas.rect(),
              rmlib::fb::Waveform::GC16,
              rmlib::fb::UpdateFlags::FullRefresh);
  return true;
}

void
updateRect(rmlib::Rect& rect, int x, int y) {
  rect.topLeft.x = std::min(rect.topLeft.x, x);
  rect.topLeft.y = std::min(rect.topLeft.y, y);
  rect.bottomRight.x = std::max(rect.bottomRight.x, x);
  rect.bottomRight.y = std::max(rect.bottomRight.y, y);
}

bool
findLcd() {
  // find first red pixel
  skin->canvas.forEach([](int x, int y, int val) {
    if ((val & 0x00ff00) == 0x00ff00) {
      if (lcd_rect.has_value()) {
        updateRect(*lcd_rect, x, y);
      } else {
        lcd_rect = { { x, y }, { x, y } };
      }
    }
  });

  if (lcd_rect.has_value()) {
    std::cout << "LCD rect: " << *lcd_rect << std::endl;
    return true;
  }

  return false;
}

bool
loadKeymap() {
  findLcd();

  return true;
}

int
get_scancode(int x, int y) {
  if (x >= skin->canvas.width || y >= skin->canvas.height) {
    return -1;
  }

  auto pixel = skin->canvas.getPixel(x, y);
  auto code = (pixel & 0x00ff00) >> 8;

  // TODO: move to better location.
  if (code == 0xfe) {
    shouldStop = true;
    return -1;
  }

  if (code == 0 || code > TILEM_KEY_DEL) {
    return -1;
  }
  return code;
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
  std::cout << " at " << ev.location;
  std::cout << " dist " << ev.distance << " pres " << ev.pressure << std::endl;
}

void
handleEvent(rmlib::fb::FrameBuffer& fb, rmlib::input::Event ev) {
  static int scanCodes[10];
  if (std::holds_alternative<rmlib::input::KeyEvent>(ev)) {
    std::cout << "key ev " << std::get<rmlib::input::KeyEvent>(ev).keyCode
              << std::endl;
    return;
  }

  if (std::holds_alternative<rmlib::input::PenEvent>(ev)) {
    static int scancode = -1;
    auto penEv = std::get<rmlib::input::PenEvent>(ev);
    if (penEv.type != PenEvent::Move) {
      printEvent(penEv);
    }

    if (penEv.type == PenEvent::TouchDown) {

      if (scancode != -1) {
        tilem_keypad_release_key(calc, scancode);
      }

      scancode = get_scancode(penEv.location.x, penEv.location.y);
      std::cout << "pen down " << scancode << std::endl;

      if (scancode != -1) {
        tilem_keypad_press_key(calc, scancode);
      }
    } else if (penEv.type == PenEvent::TouchUp/*penDown == true &&
               (penEv.pressure == 0 ||
                penEv.type == rmlib::input::PenEvent::TouchUp ||
                penEv.type == rmlib::input::PenEvent::ToolLeave)*/) {
      std::cout << "pen up " << scancode << std::endl;
      if (scancode != -1) {
        tilem_keypad_release_key(calc, scancode);
        scancode = -1;
      }
    }
    return;
  }

  auto touchEv = std::get<rmlib::input::TouchEvent>(ev);
  if (touchEv.type == rmlib::input::TouchEvent::Down) {
    auto scancode = get_scancode(touchEv.location.x, touchEv.location.y);
    std::cout << "touch down " << scancode << std::endl;
    scanCodes[touchEv.slot] = scancode;
    if (scancode != -1) {
      tilem_keypad_press_key(calc, scancode);
    } else if (lcd_rect->contains(touchEv.location)) {
      std::cout << "lcd redraw\n";
      showCalculator(fb);
    }
  } else if (touchEv.type == rmlib::input::TouchEvent::Up) {
    auto scancode = scanCodes[touchEv.slot];
    std::cout << "touch up " << scancode << std::endl;
    if (scancode != -1) {
      tilem_keypad_release_key(calc, scancode);
    }
  }
}

std::chrono::steady_clock::time_point
getTime() {
  return std::chrono::steady_clock::now();
}

const auto FPS = 100;
const auto TPS = std::chrono::milliseconds(1000) / FPS;

const auto frame_time = std::chrono::milliseconds(50); // 50 ms ->  20 fps

void
frameCallback(TilemCalc* calc, void* data) {
  auto* fb = reinterpret_cast<fb::FrameBuffer*>(data);
  static auto* lcd = [] {
    auto* lcd = tilem_lcd_buffer_new();
    if (lcd == nullptr) {
      perror("Error alloc lcd bufer");
      std::exit(-1);
    }
    return lcd;
  }();

  tilem_lcd_get_frame(calc, lcd);

  float scale_x = (float)lcd_rect->width() / lcd->width;
  float scale_y = (float)lcd_rect->height() / lcd->height;
  fb->canvas.transform(
    [&](int x, int y, int) {
      int subY = (y - lcd_rect->topLeft.y) / scale_y;
      int subX = (x - lcd_rect->topLeft.x) / scale_x;
      uint8_t data = lcd->data[subY * lcd->rowstride + subX];
      uint8_t pixel = lcd->contrast == 0 ? 0 : data ? 0 : 0xff;
      return (pixel / 16) << 1;
    },
    *lcd_rect);

  fb->doUpdate(
    *lcd_rect, rmlib::fb::Waveform::DU, rmlib::fb::UpdateFlags::None);
}

void
intHandler(int sig) {
  shouldStop = true;
}
} // namespace

int
main(int argc, char* argv[]) {
  auto fb = rmlib::fb::FrameBuffer::open();
  if (fb.isError()) {
    std::cerr << fb.getError().msg << std::endl;
    return -1;
  }

  rmlib::input::InputManager input;

  if (auto err = input.openAll(); err.isError()) {
    std::cerr << "Error opening input: " + err.getError().msg + "\n";
    return -1;
  }

  if (!showCalculator(*fb)) {
    return -1;
  }

  if (!loadKeymap()) {
    return -1;
  }

  calc = tilem_calc_new(TILEM_CALC_TI84P);
  if (calc == nullptr) {
    std::cerr << "Error init calc\n";
    return -1;
  }

  const auto* calc_name = argc > 1 ? argv[1] : calc_default_rom;

  FILE* rom = fopen(calc_name, "r");
  if (rom == nullptr) {
    perror("Error opening rom file");
    return -1;
  }

  const auto save_name = std::string(calc_name) + calc_save_extension;
  FILE* save = fopen(save_name.c_str(), "r");
  if (save == nullptr) {
    perror("No save file");
  }

  if (tilem_calc_load_state(calc, rom, save) != 0) {
    perror("Error reading rom");
    return -1;
  }

  fclose(rom);
  if (save != nullptr) {
    fclose(save);
  }

  tilem_z80_add_timer(calc,
                      std::chrono::microseconds(frame_time).count(),
                      std::chrono::microseconds(frame_time).count(),
                      /* real time */ 1,
                      frameCallback,
                      &*fb);

  std::cout << "loaded rom, entering mainloop\n";

  std::signal(SIGINT, intHandler);

  auto lastUpdateT = getTime();
  while (!shouldStop) {
    constexpr auto wait_time = std::chrono::milliseconds(10);
    auto eventsOrErr = input.waitForInput(wait_time);
    if (eventsOrErr.isError()) {
      std::cerr << eventsOrErr.getError().msg << std::endl;
    } else {
      for (const auto& event : *eventsOrErr) {
        handleEvent(*fb, event);
      }
    }

    const auto time = getTime();
    auto diff = time - lastUpdateT;
    if (diff > TPS) {
      // Skip frames if we were paused.
      if (diff > std::chrono::seconds(1)) {
        diff = TPS;
      }

      tilem_z80_run_time(
        calc,
        std::chrono::duration_cast<std::chrono::microseconds>(diff).count(),
        nullptr);
      lastUpdateT = time;
    }
  }

  std::cout << "Saving state\n";
  save = fopen(save_name.c_str(), "w");
  if (save == nullptr) {
    perror("Error opening save file");
    return -1;
  }

  tilem_calc_save_state(calc, nullptr, save);

  return 0;
}
