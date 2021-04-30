#include "FrameBuffer.h"
#include "Device.h"

#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef EMULATE
#include <SDL.h>
#include <chrono>
// #include <SDL2/SDL.h>
#else
#include "mxcfb.h"
#endif

namespace rmlib::fb {
namespace {
#ifndef EMULATE
constexpr auto shm_path = "/dev/shm/swtfb.01";
constexpr auto fb_path = "/dev/fb0";

constexpr int msg_q_id = 0x2257c;

// TODO: import this from rm2-fb
struct msgq_msg {
  long type = 2;

  mxcfb_update_data update;
};

// Global msgq:
int msqid = -1;

#else

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

  const auto getGrey = [&canvas](int x, int y) {
    const auto rgb = *canvas.getPtr<uint16_t>(x, y);
    const auto r = rgb & 0x1f;
    return r << 3;
  };

  static int color = 0;
  color = (color + 1) % 3;
  const auto surfStart = region.topLeft / EMULATE_SCALE;
  const auto surfEnd = region.bottomRight / EMULATE_SCALE;

  for (int y = surfStart.y; y <= surfEnd.y; y++) {
    for (int x = surfStart.x; x <= surfEnd.x; x++) {

      auto pixel = getGrey(x * EMULATE_SCALE, y * EMULATE_SCALE);
#if EMULATE_SCALE > 1
      pixel += getGrey(x * EMULATE_SCALE + 1, y * EMULATE_SCALE);
      pixel += getGrey(x * EMULATE_SCALE, y * EMULATE_SCALE + 1);
      pixel += getGrey(x * EMULATE_SCALE + 1, y * EMULATE_SCALE + 1);
      pixel /= 4;
#endif

      // assume rgb565
      int r = pixel;
      int g = pixel;
      int b = pixel;

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
#endif // EMULATE
} // namespace

ErrorOr<FrameBuffer>
FrameBuffer::open() {
#ifdef EMULATE
  auto canvas = TRY(makeEmulatedCanvas());
  return FrameBuffer(FrameBuffer::Swtcon, 1337, canvas);
#else
  int components = sizeof(uint16_t);
  int width;
  int height;
  int stride;

  auto devType = TRY(device::getDeviceType());
  const auto fbType = [&, devType] {
    switch (devType) {
      default:
      case device::DeviceType::reMarkable1:
        std::cerr << "rM1 currently untested, please open a github issue if "
                     "you encounter any issues\n";

        width = 1404; // TODO: use ioctl
        height = 1872;
        stride = 1408 * components;

        return rM1;
      case device::DeviceType::reMarkable2:

        width = 1404;
        height = 1872;
        stride = width * components;

        if (getenv("RM2FB_SHIM") != nullptr) {
          std::cerr << "Using rm2fb shim\n";
          return Shim;
        }

        // Not all of the rm2 fb modes support retrieving the size, so hard code
        // it here.

        // check if shared mem exists
        if (access(shm_path, F_OK) == 0) {
          std::cerr << "Using rm2fb ipc\n";
          return rM2fb;
        }

        // start swtcon
        return Swtcon;
    }
  }();

  if (fbType == Swtcon) {
    return Error{ "No rm2fb found, swtcon not implemented yet\n" };
  }

  const auto* path = [fbType] {
    switch (fbType) {
      case rM1:
      case Shim:
        return fb_path;

      case rM2fb:
        return shm_path;

      case Swtcon:
      default:
        assert(false);
        return "";
    }
  }();

  auto fd = ::open(path, O_RDWR);
  if (fd < 0) {
    perror("error");
    return Error{ "Error opening " + std::string(path) };
  }

  auto* memory = static_cast<uint8_t*>(
    mmap(nullptr, stride * height, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

  if (memory == nullptr) {
    ::close(fd);
    return Error{ "Error mapping fb" };
  }

  if (fbType == rM2fb) {
    msqid = msgget(msg_q_id, IPC_CREAT | 0600);
    if (msqid < 0) {
      perror("Error opening msgq");
      ::close(fd);
      return Error{ "Error open message queue" };
    }
  }

  Canvas canvas(memory, width, height, stride, components);
  return FrameBuffer(fbType, fd, canvas);
#endif
}

void
FrameBuffer::close() {
#ifdef EMULATE
  if (fd == 1337) {
    SDL_DestroyWindow(window);
    window = nullptr;
    fd = -1;
  }
#else

  if (canvas.getMemory() != nullptr) {
    munmap(canvas.getMemory(), canvas.totalSize());
  }
  canvas = Canvas{};

  if (fd != -1) {
    ::close(fd);
  }
  fd = -1;

#endif
}

FrameBuffer::~FrameBuffer() {
  close();
}

void
FrameBuffer::doUpdate(Rect region, Waveform waveform, UpdateFlags flags) const {
#ifdef EMULATE
  std::cout << (waveform == Waveform::DU ? "DU" : "Other") << " ";
  updateEmulatedCanvas(canvas, region);
#else
  if (type != Swtcon) {
    auto update = mxcfb_update_data{};

    update.waveform_mode = static_cast<int>(waveform);
    update.update_mode = (flags & UpdateFlags::FullRefresh) != 0 ? 1 : 0;

#define TEMP_USE_REMARKABLE_DRAW 0x0018
#define EPDC_FLAG_EXP1 0x270ce20
    update.update_marker = 0;
    update.dither_mode = EPDC_FLAG_EXP1;
    update.temp = TEMP_USE_REMARKABLE_DRAW;
    update.flags = 0;

    update.update_region.left = region.topLeft.x;
    update.update_region.top = region.topLeft.y;
    update.update_region.width = region.width();
    update.update_region.height = region.height();

#ifndef NDEBUG
    const auto waveformStr = [waveform] {
      switch (waveform) {
        case Waveform::DU:
          return "DU";
        case Waveform::GC16:
          return "GC16";
        case Waveform::GC16Fast:
          return "GC16Fast";
        default:
          return "???";
      }
    }();
    std::cerr << "UPDATE " << waveformStr << " region: {"
              << update.update_region.left << " "
              << " " << update.update_region.top << " "
              << update.update_region.width << " "
              << update.update_region.height << "}\n";
#endif

    if (type == rM2fb) {
      auto msg = msgq_msg{};
      msg.update = update;

      if (msgsnd(msqid, reinterpret_cast<void*>(&msg), sizeof(msgq_msg), 0) !=
          0) {
        perror("Error sending update msg");
      }
    } else {
      ioctl(fd, MXCFB_SEND_UPDATE, &update);
    }
  } else {
    assert(false && "Unimplemented");
  }
#endif
}

} // namespace rmlib::fb
