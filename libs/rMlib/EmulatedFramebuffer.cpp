#include "Canvas.h"
#include "FrameBuffer.h"

#include <SDL.h>
#include <array>
#include <chrono>

#include <atomic>
#include <iostream>
#include <signal.h>
#include <thread>

#if __linux__
#include <libevdev/libevdev-uinput.h>
#endif

namespace rmlib::fb {
namespace {
constexpr auto canvas_width = 1404;
constexpr auto canvas_height = 1872;
constexpr auto canvas_components = 2;
constexpr auto window_width = canvas_width / EMULATE_SCALE;
constexpr auto window_height = canvas_height / EMULATE_SCALE;

// Global window for emulation.
SDL_Window* window = nullptr;
std::unique_ptr<uint8_t[]> emuMem;

void
putpixel(SDL_Surface* surface, int x, int y, Uint32 pixel) {
  if (x < 0 || x >= window_width || y < 0 || y >= window_height) {
    return;
  }

  int bpp = surface->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to set */
  Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;

  switch (bpp) {
    case 1:
      *p = pixel;
      break;

    case 2:
      *(Uint16*)p = pixel;
      break;

    case 3:
      if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
        p[0] = (pixel >> 16) & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = pixel & 0xff;
      } else {
        p[0] = pixel & 0xff;
        p[1] = (pixel >> 8) & 0xff;
        p[2] = (pixel >> 16) & 0xff;
      }
      break;

    case 4:
      *(Uint32*)p = pixel;
      break;
  }
}

ErrorOr<Canvas>
makeEmulatedCanvas() {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    return Error{ std::string("could not initialize sdl2:") + SDL_GetError() };
  }
  window = SDL_CreateWindow("rM emulator",
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            window_width,
                            window_height,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == NULL) {
    return Error{ std::string("could not create window:") + SDL_GetError() };
  }
  auto* screenSurface = SDL_GetWindowSurface(window);
  SDL_FillRect(

    screenSurface,
    NULL,
    SDL_MapRGB(screenSurface->format, 0x1F << 3, 0x1F << 3, 0x1F << 3));
  SDL_UpdateWindowSurface(window);

  const auto memSize = canvas_width * canvas_height * canvas_components;
  emuMem = std::make_unique<uint8_t[]>(memSize);
  memset(emuMem.get(), 0xaa, memSize);
  return Canvas(emuMem.get(), canvas_width, canvas_height, canvas_components);
}

void
updateEmulatedCanvas(const Canvas& canvas, Rect region) {
  std::cout << "Update: " << region << "\n";
  auto* surface = SDL_GetWindowSurface(window);

  const auto getPixel = [&canvas](int x, int y) {
    const auto rgb = *canvas.getPtr<uint16_t>(x, y);
    const auto b = (rgb & 0x1f) << 3;
    const auto g = ((rgb >> 5) & 0x3f) << 2;
    const auto r = ((rgb >> 11) & 0x1f) << 3;
    return std::array{ r, g, b };
  };

  static int color = 0;
  color = (color + 1) % 3;
  const auto surfStart = region.topLeft / EMULATE_SCALE;
  const auto surfEnd = region.bottomRight / EMULATE_SCALE;

  for (int y = surfStart.y; y <= surfEnd.y; y++) {
    for (int x = surfStart.x; x <= surfEnd.x; x++) {

      auto pixel = getPixel(x * EMULATE_SCALE, y * EMULATE_SCALE);
#if EMULATE_SCALE > 1
      const auto pixel1 = getPixel(x * EMULATE_SCALE + 1, y * EMULATE_SCALE);
      const auto pixel2 = getPixel(x * EMULATE_SCALE, y * EMULATE_SCALE + 1);
      const auto pixel3 =
        getPixel(x * EMULATE_SCALE + 1, y * EMULATE_SCALE + 1);

      pixel = {
        (pixel[0] + pixel1[0] + pixel2[0] + pixel3[0]) / 4,
        (pixel[1] + pixel1[1] + pixel2[1] + pixel3[1]) / 4,
        (pixel[2] + pixel1[2] + pixel2[2] + pixel3[2]) / 4,
      };
#endif

      // assume rgb565
      int r = pixel[0];
      int g = pixel[1];
      int b = pixel[2];

      if (y == surfStart.y || y == surfEnd.y || x == surfStart.x ||
          x == surfEnd.x) {
        r = color == 2 ? 0xff : 0x00;
        g = color == 1 ? 0xff : 0x00;
        b = color == 0 ? 0xff : 0x00;
      }

      putpixel(surface, x, y, SDL_MapRGB(surface->format, r, g, b));
    }
  }

  SDL_Rect rect;
  rect.x = region.topLeft.x / EMULATE_SCALE;
  rect.y = region.topLeft.y / EMULATE_SCALE;
  rect.w = region.width() / EMULATE_SCALE + 1;
  rect.h = region.height() / EMULATE_SCALE + 1;
  SDL_UpdateWindowSurfaceRects(window, &rect, 1);
}

const char*
getStr(Waveform wave) {
  switch (wave) {
    case Waveform::DU:
      return "DU";
    case Waveform::GC16:
      return "GC16";
    case Waveform::GC16Fast:
      return "GC16Fast";
  }
}

#if __linux__
std::thread inputThread;
std::atomic_bool stop_input = false;
std::atomic_bool input_ready = false;

struct libevdev_uinput*
makeDevice() {
  auto* dev = libevdev_new();
  libevdev_set_name(dev, "test device");
  libevdev_enable_event_type(dev, EV_ABS);
  struct input_absinfo info;
  info.minimum = -1;
  info.maximum = 10;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_SLOT, &info);
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID, &info);

  info.maximum = 0;
  info.maximum = 1872;
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_X, &info);
  libevdev_enable_event_code(dev, EV_ABS, ABS_MT_POSITION_Y, &info);

  struct libevdev_uinput* uidev;
  auto err = libevdev_uinput_create_from_device(
    dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
  if (err != 0) {
    perror("uintput");
    std::cerr << "Error making uinput device\n";
    return nullptr;
  }

  std::cout << "Added sdl uintput device\n";

  return uidev;
}

void
uinput_thread() {
  auto* uidev = makeDevice();
  input_ready = true;
  if (uidev == nullptr) {
    return;
  }
  bool down = false;
  bool quit = false;

  while (!stop_input) {
    SDL_Event event;
    SDL_WaitEventTimeout(&event, 500);

    switch (event.type) {
      case SDL_QUIT:
        stop_input = true;
        quit = true;
        break;
      case SDL_MOUSEMOTION:
        if (down) {
          auto x = event.motion.x * EMULATE_SCALE;
          auto y = event.motion.y * EMULATE_SCALE;
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_POSITION_X, x);
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_POSITION_Y, y);
          libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == SDL_BUTTON_LEFT) {
          auto x = event.button.x * EMULATE_SCALE;
          auto y = event.button.y * EMULATE_SCALE;
          down = true;

          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 1);
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_POSITION_X, x);
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_POSITION_Y, y);
          libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT) {
          auto x = event.button.x * EMULATE_SCALE;
          auto y = event.button.y * EMULATE_SCALE;
          down = false;

          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, -1);
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_POSITION_X, x);
          libevdev_uinput_write_event(uidev, EV_ABS, ABS_MT_POSITION_Y, y);
          libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
        }
        break;
    }
  }

  libevdev_uinput_destroy(uidev);
  if (quit) {
    kill(getpid(), SIGINT);
  }
}
#endif
} // namespace

ErrorOr<FrameBuffer>
FrameBuffer::open() {
  auto canvas = TRY(makeEmulatedCanvas());

#if __linux__
  inputThread = std::thread(uinput_thread);
  while (!input_ready) {
    usleep(200);
  }
#endif

  return FrameBuffer(FrameBuffer::rM2fb, 1337, canvas);
}

void
FrameBuffer::close() {
  if (fd == 1337) {
#if __linux__
    stop_input = true;
    inputThread.join();
#endif

    SDL_DestroyWindow(window);
    window = nullptr;
    fd = -1;
  }
}

void
FrameBuffer::doUpdate(Rect region, Waveform waveform, UpdateFlags flags) const {
  std::cout << getStr(waveform) << " ";
  updateEmulatedCanvas(canvas, region);
}

} // namespace rmlib::fb
