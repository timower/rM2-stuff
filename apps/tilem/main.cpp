#include "mxcfb.h"

#define LCD_WIDTH 128
#define LCD_HEIGHT 64

#include "tilem.h"

#include <iostream>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/input.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

constexpr auto screen_width = 1404;
constexpr auto screen_height = 1872;
constexpr auto mode_du = 1;
constexpr auto mode_hd = 2;
constexpr auto mode_gray = 3;

int fbFd = -1;
int inputFd = -1;
uint16_t* fbMem = nullptr;

int keymap_width;
int keymap_height;
int keymap_bytepp;
uint8_t* keymapMem = nullptr;

// TODO: don't hardcode this
constexpr auto keymap_scale = 2;

int lcd_x = -1;
int lcd_y = -1;

TilemCalc* calc = nullptr;

constexpr auto fb_path = "/dev/fb0";
constexpr auto input_path = "/dev/input/event2";
constexpr auto calc_skin_path = "ti84p.png";
constexpr auto calc_key_path = "ti84p-key.png";

bool
open_fb() {
  fbFd = open(fb_path, O_RDWR);
  if (fbFd < 0) {
    std::cerr << "Cannot open fb\n";
    return false;
  }

  fbMem = (uint16_t*)mmap(nullptr,
                          screen_height * screen_width * sizeof(uint16_t),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          fbFd,
                          0);
  if (fbMem == nullptr) {
    std::cerr << "Cannot map fb\n";
    return false;
  }

  return true;
}

void
close_fb() {
  munmap(fbMem, 0);
  close(fbFd);
}

void
doUpdate(int x, int y, int width, int height, int waveform, bool full) {
  mxcfb_update_data update;
  update.waveform_mode = waveform;
  update.update_mode = full ? 1 : 0;
  update.update_region.top = y;
  update.update_region.left = x;
  update.update_region.width = width;
  update.update_region.height = height;

  ioctl(fbFd, MXCFB_SEND_UPDATE, &update);
}

void
clearScreen() {
  for (int y = 0; y < screen_height; y++) {
    for (int x = 0; x < screen_width; x++) {
      fbMem[y * screen_width + x] = 0xFFFF; // white
    }
  }
  doUpdate(0, 0, screen_width, screen_height, mode_hd, true);
}

bool
showCalculator() {
  int imgWidth;
  int imgHeight;
  int n;
  auto* image = stbi_load(calc_skin_path, &imgWidth, &imgHeight, &n, 1);
  if (image == nullptr) {
    std::cerr << "Error loading image\n";
    return false;
  }

  std::cout << "calc skin size: " << imgWidth << "x" << imgHeight << std::endl;

  for (int y = 0; y < imgHeight; y++) {
    for (int x = 0; x < imgWidth; x++) {
      auto pixel = image[y * imgWidth + x];
      fbMem[y * screen_width + x] = (pixel / 16) << 1;
    }
  }

  doUpdate(0, 0, imgWidth, imgHeight, mode_gray, true);
  stbi_image_free(image);
  return true;
}

bool
findLcd() {
  // find first red pixel
  for (int y = 0; y < keymap_height; y++) {
    for (int x = 0; x < keymap_width; x++) {
      auto* pixel =
        &keymapMem[y * keymap_width * keymap_bytepp + x * keymap_bytepp];
      if (pixel[0] == 0xff && pixel[1] == 0 && pixel[2] == 0) {
        lcd_x = x * keymap_scale;
        lcd_y = y * keymap_scale;
        return true;
      }
    }
  }
  return false;
}

bool
loadKeymap() {
  auto* image =
    stbi_load(calc_key_path, &keymap_width, &keymap_height, &keymap_bytepp, 0);
  if (image == nullptr) {
    std::cerr << "Error loading keymap\n";
    return false;
  }
  keymapMem = image;

  std::cout << "keymap bpp: " << keymap_bytepp << std::endl;

  findLcd();

  std::cout << "LCD pos: " << lcd_x << " " << lcd_y << std::endl;

  return true;
}

bool
openInput() {

  inputFd = open(input_path, O_RDONLY);
  if (inputFd < 0) {
    std::cerr << "Cannot open touch input\n";
    return false;
  }
  return true;
}

struct SlotInfo {
  int id = -5;
  int x = 0;
  int y = 0;

  int group;
  int bit;
};

std::pair<int, int>
get_key(int x, int y) {
  int scaledX = x / keymap_scale;
  int scaledY = y / keymap_scale;
  if (scaledX >= keymap_width || scaledY >= keymap_height) {
    return { -1, -1 };
  }

  auto* pixel = &keymapMem[scaledY * keymap_width * keymap_bytepp +
                           scaledX * keymap_bytepp];

  if (pixel[0] == 0xff) {
    return { -1, -1 };
  }

  int group = pixel[1] >> 4;
  int bit = pixel[2] >> 4;
  if (group > 7 || bit > 7) {
    return { -1, -1 };
  }

  return { group, bit };
}

bool
read_input() {
  static int slot = 0;
  static SlotInfo slots[10];

  bool newId = false;
  while (true) {
    input_event event;
    auto size = read(inputFd, &event, sizeof(input_event));
    if (size != sizeof(input_event)) {
      std::cerr << "Error reading: " << size << std::endl;
      return false;
    }

    if (event.type == EV_SYN && event.code == SYN_REPORT) {
      break;
    } else if (event.type == EV_ABS) {
      if (event.code == ABS_MT_POSITION_X) {
        slots[slot].x = event.value;
      } else if (event.code == ABS_MT_POSITION_Y) {
        slots[slot].y = screen_height - event.value;
      } else if (event.code == ABS_MT_TRACKING_ID) {
        newId = slots[slot].id != event.value;
        slots[slot].id = event.value;
      } else if (event.code == ABS_MT_SLOT) {
        slot = event.value;
      }
    }
  }

  if (slots[slot].id == -1) {
    // Touch up
    auto scancode = slots[slot].group << 3 | (slots[slot].bit + 1);
    std::cout << "touch up " << slots[slot].group << " " << slots[slot].bit
              << " scancode " << scancode << std::endl;
    if (slots[slot].group != -1 && calc != nullptr) {
      tilem_keypad_release_key(calc, scancode);
      // calcinterface_release_key(slots[slot].group, slots[slot].bit);
    }
  } else if (newId) {
    auto [group, bit] = get_key(slots[slot].x, slots[slot].y);
    slots[slot].group = group;
    slots[slot].bit = bit;
    auto scancode = slots[slot].group << 3 | (slots[slot].bit + 1);
    std::cout << "Touch down " << slot << " at " << slots[slot].x << " "
              << slots[slot].y << " group " << group << " " << bit
              << " scancode " << scancode << std::endl;
    if (group != -1 && calc != nullptr) {
      tilem_keypad_press_key(calc, scancode);
      // calcinterface_press_key(group, bit);
    }
  }

  return true;
}

void
close_input() {
  close(inputFd);
}

float
getTime() {
  timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  const double result = (double)tv.tv_sec + (double)tv.tv_nsec / 1.0e9;
  return result;
}

const auto FPS = 50;
const auto TPS = 1.0f / FPS;

int
main(int argc, char* argv[]) {
  if (getenv("RM2FB_SHIM") == nullptr) {
    std::cerr << "REQUIRES SHIM\n";
    return -1;
  }

  if (!open_fb()) {
    return -1;
  }

  if (!openInput()) {
    return -1;
  }

  // clearScreen();
  if (!showCalculator()) {
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

  if (tilem_calc_load_state(calc, rom, nullptr) != 0) {
    perror("Error reading rom");
    return -1;
  }
  fclose(rom);

  auto* lcd = tilem_lcd_buffer_new();
  if (lcd == nullptr) {
    perror("Error alloc lcd bufer");
    return -1;
  }

  // if (calcinterface_getmodel() != 6) {
  //   std::cerr << "Error loading rom\n";
  //   return -1;
  // }

  // calcinterface_reset();

  std::cout << "loaded rom, entering mainloop\n";

  auto lastUpdateT = getTime();
  float diff = 0;
  while (true) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(inputFd, &fds);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 40 * 1000; // 33 ms

    auto ret = select(inputFd + 1, &fds, nullptr, nullptr, &tv);
    if (ret < 0) {
      perror("input select failed");
      break;
    }

    if (ret > 0) {
      read_input();
    }

    const auto time = getTime();
    diff += (time - lastUpdateT);
    lastUpdateT = time;
    while (diff > TPS) {
      tilem_z80_run_time(calc, TPS * 1000000, nullptr);
      diff -= TPS;
    }

    tilem_lcd_get_frame(calc, lcd);
    std::cout << "cont: " << (int)lcd->contrast << std::endl;

    for (int y = 0; y < lcd->height; y++) {
      for (int x = 0; x < lcd->width; x++) {
        uint8_t pixel =
          lcd->contrast == 0 ? 0 : 0xff - lcd->data[y * lcd->rowstride + x];
        fbMem[(y + lcd_y) * screen_width + (x + lcd_x)] = (pixel / 16) << 1;
      }
    }
    doUpdate(lcd_x, lcd_y, LCD_WIDTH, LCD_HEIGHT, mode_gray, false);
    // free(screen);
  }

  close_fb();
  close_input();
  return 0;
}
