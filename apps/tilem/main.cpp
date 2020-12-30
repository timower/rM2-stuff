#include <FrameBuffer.h>
#include <Input.h>

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

using namespace rmlib;
using namespace rmlib::input;

volatile std::atomic_bool shouldStop = false;

int inputFd = -1;

std::optional<rmlib::ImageCanvas> keymap;

// TODO: don't hardcode this
constexpr auto keymap_scale = 2;

std::optional<rmlib::Rect> lcd_rect = std::nullopt;

TilemCalc* calc = nullptr;

constexpr auto calc_skin_path = "ti84p.png";
constexpr auto calc_key_path = "ti84p-key.png";
constexpr auto calc_save_name = "ti84p.sav";

void
clearScreen(rmlib::fb::FrameBuffer& fb) {
  fb.canvas.set(0xFFFF);
  fb.doUpdate(fb.canvas.rect(),
              rmlib::fb::Waveform::GC16,
              rmlib::fb::UpdateFlags::FullRefresh);
}

bool
showCalculator(rmlib::fb::FrameBuffer& fb) {
  auto image = rmlib::ImageCanvas::load(calc_skin_path, 1);
  if (!image.has_value()) {
    std::cerr << "Error loading image\n";
    return false;
  }

  std::cout << "calc skin size: " << image->canvas.width << "x"
            << image->canvas.height << std::endl;

  rmlib::transform(fb.canvas,
                   { 0, 0 },
                   image->canvas,
                   image->canvas.rect(),
                   [](int x, int y, int val) { return (val / 16) << 1; });

  fb.doUpdate(image->canvas.rect(),
              rmlib::fb::Waveform::GC16Fast,
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
  keymap->canvas.forEach([](int x, int y, int val) {
    if ((val & 0xffffff) == 0x0000ff) {
      if (lcd_rect.has_value()) {
        updateRect(*lcd_rect, x, y);
      } else {
        lcd_rect = { { x, y }, { x, y } };
      }
    }
  });

  if (lcd_rect.has_value()) {
    *lcd_rect *= keymap_scale;

    std::cout << "LCD pos: " << lcd_rect->topLeft.x << " "
              << lcd_rect->topLeft.y << std::endl;
    std::cout << "LCD end: " << lcd_rect->bottomRight.x << " "
              << lcd_rect->bottomRight.y << std::endl;
    return true;
  }

  return false;
}

bool
loadKeymap() {
  auto image = rmlib::ImageCanvas::load(calc_key_path);
  if (!image.has_value()) {
    std::cerr << "Error loading keymap\n";
    return false;
  }
  std::cout << "keymap bpp: " << image->canvas.components << std::endl;
  keymap = std::move(image);

  findLcd();

  return true;
}

constexpr int
toScanCode(int group, int bit) {
  auto scancode = (group << 3) | bit;
  return scancode + 1;
}

int
get_scancode(int x, int y) {
  int scaledX = x / keymap_scale;
  int scaledY = y / keymap_scale;
  if (scaledX >= keymap->canvas.width || scaledY >= keymap->canvas.height) {
    return -1;
  }

  auto pixel = keymap->canvas.getPixel(scaledX, scaledY);
  if ((pixel & 0xff) == 0xff) {
    return -1;
  }

  int group = ((pixel >> 8) & 0xff) >> 4;
  int bit = ((pixel >> 16) & 0xff) >> 4;
  if (group > 7 || bit > 7) {
    return -1;
  }

  return toScanCode(group, bit);
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
handleEvent(rmlib::fb::FrameBuffer& fb, rmlib::input::Event ev) {
  static int scanCodes[10];
  if (std::holds_alternative<rmlib::input::KeyEvent>(ev)) {
    std::cout << "key ev " << std::get<rmlib::input::KeyEvent>(ev).keyCode
              << std::endl;
    return;
  }

  if (std::holds_alternative<rmlib::input::PenEvent>(ev)) {
    static int scancode = -1;
    static bool penDown = false;
    auto penEv = std::get<rmlib::input::PenEvent>(ev);
    if (penEv.type != PenEvent::Move) {
      printEvent(penEv);
    }

    if (penEv.type == PenEvent::TouchDown) {
      penDown = true;

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
      penDown = false;
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

float
getTime() {
  timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  const double result = (double)tv.tv_sec + (double)tv.tv_nsec / 1.0e9;
  return result;
}

const auto FPS = 100;
const auto TPS = 1.0f / FPS;

const auto frame_time = 50 * 1000; // 50 ms in us, 20 fps

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

int
main(int argc, char* argv[]) {
  auto fb = rmlib::fb::FrameBuffer::open();
  if (!fb.has_value()) {
    return -1;
  }

  rmlib::input::InputManager input;

  if (!input.openAll()) {
    std::cerr << "Error opening input\n";
    return -1;
  }

  // clearScreen();
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

  FILE* rom = fopen("ti84p.rom", "r");
  if (rom == nullptr) {
    perror("Error opening rom file");
    return -1;
  }

  FILE* save = fopen(calc_save_name, "r");
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
                      frame_time,
                      frame_time,
                      /* real time */ 1,
                      frameCallback,
                      &fb.value());

  std::cout << "loaded rom, entering mainloop\n";

  std::signal(SIGINT, intHandler);

  auto lastUpdateT = getTime();
  while (!shouldStop) {
    auto event = input.waitForInput(0.01f);
    if (event.has_value()) {
      handleEvent(*fb, *event);
    }

    const auto time = getTime();
    const auto diff = time - lastUpdateT;
    if (diff > TPS) {
      tilem_z80_run_time(calc, diff * 1000000, nullptr);
      lastUpdateT = time;
    }
  }

  std::cout << "Saving state\n";
  save = fopen(calc_save_name, "w");
  if (save == nullptr) {
    perror("Error opening save file");
    return -1;
  }

  tilem_calc_save_state(calc, nullptr, save);

  return 0;
}
