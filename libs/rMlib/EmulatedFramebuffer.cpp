#include "Canvas.h"
#include "FrameBuffer.h"

#include <SDL.h>

#include <array>
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

#if EMULATE_UINPUT
#include <libevdev/libevdev-uinput.h>
#endif

bool rmlibDisableWindow = false; // NOLINT

namespace rmlib::fb {
namespace {

constexpr auto canvas_components = 2;

constexpr int emulated_fd = 1337;

// Global window for emulation.
SDL_Window* window = nullptr;      // NOLINT
std::unique_ptr<uint8_t[]> emuMem; // NOLINT

void
putpixel(SDL_Surface* surface, int x, int y, Uint32 pixel) {
  if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) {
    return;
  }

  int bpp = surface->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to set */
  auto* p = // NOLINTNEXTLINE
    reinterpret_cast<Uint8*>(surface->pixels) + y * surface->pitch + x * bpp;
  memcpy(p, &pixel, bpp);
}

ErrorOr<Canvas>
makeEmulatedCanvas(Size size) {
  if (!rmlibDisableWindow) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      return Error::make(std::string("could not initialize sdl2:") +
                         SDL_GetError());
    }

    window = SDL_CreateWindow("rM emulator",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              size.width / EMULATE_SCALE,
                              size.height / EMULATE_SCALE,
                              SDL_WINDOW_SHOWN);
    if (window == nullptr) {
      return Error::make(std::string("could not create window:") +
                         SDL_GetError());
    }

    constexpr auto clear_color = 0x1f << 3;
    auto* screenSurface = SDL_GetWindowSurface(window);
    SDL_FillRect(
      screenSurface,
      nullptr,
      SDL_MapRGB(screenSurface->format, clear_color, clear_color, clear_color));
    SDL_UpdateWindowSurface(window);
  }

  constexpr auto clear_color = 0xaa;
  const auto memSize = size.width * size.height * canvas_components;
  emuMem = std::make_unique<uint8_t[]>(memSize); // NOLINT
  memset(emuMem.get(), clear_color, memSize);
  return Canvas(emuMem.get(), size.width, size.height, canvas_components);
}

void
updateEmulatedCanvas(const Canvas& canvas, Rect region) {
  if (rmlibDisableWindow) {
    return;
  }

  auto* surface = SDL_GetWindowSurface(window);

  const auto getPixel = [&canvas](int x, int y) {
    const auto rgb = canvas.getPixel(x, y);
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

      // if (y == surfStart.y || y == surfEnd.y || x == surfStart.x ||
      //     x == surfEnd.x) {
      //   r = color == 2 ? UINT8_MAX : 0x00;
      //   g = color == 1 ? UINT8_MAX : 0x00;
      //   b = color == 0 ? UINT8_MAX : 0x00;
      // }

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

std::string
getStr(Waveform wave) {
  switch (wave) {
    case Waveform::DU:
      return "DU";
    case Waveform::GC16:
      return "GC16";
    case Waveform::GC16Fast:
      return "GC16Fast";
    case Waveform::A2:
      return "A2";
    default:
      return "Unknown (" + std::to_string(int(wave)) + ")";
  }
}

#if EMULATE_UINPUT
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
FrameBuffer::open(std::optional<Size> requestedSize) {
  constexpr auto canvas_width = 1404;
  constexpr auto canvas_height = 1872;

  auto canvas = TRY(makeEmulatedCanvas(
    requestedSize.value_or(Size{ canvas_width, canvas_height })));

#if EMULATE_UINPUT
  inputThread = std::thread(uinput_thread);
  while (!input_ready) {
    usleep(200);
  }
#endif

  return FrameBuffer(FrameBuffer::rM2Stuff, unistdpp::FD(emulated_fd), canvas);
}

void
FrameBuffer::close() {
  if (fd.fd == emulated_fd) {
#if EMULATE_UINPUT
    stop_input = true;
    inputThread.join();
#endif

    SDL_DestroyWindow(window);
    window = nullptr;
    fd.fd = unistdpp::FD::invalid_fd;
  }
}

void
FrameBuffer::doUpdate(Rect region, Waveform waveform, UpdateFlags flags) const {
  updateEmulatedCanvas(canvas, region);
}

} // namespace rmlib::fb
