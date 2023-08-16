#include "FrameBuffer.h"
#include "Device.h"

#include <array>
#include <cassert>
#include <iostream>

#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "mxcfb.h"

namespace rmlib::fb {
namespace {
constexpr auto fb_path = "/dev/fb0";
} // namespace

ErrorOr<FrameBuffer>
FrameBuffer::open() {
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

        // Not all of the rm2 fb modes support retrieving the size, so hard code
        // it here.
        width = 1404;
        height = 1872;
        stride = width * components;

        if (getenv("RM2FB_SHIM") != nullptr) {
          std::cerr << "Using rm2fb shim\n";
          return Shim;
        }

        // check if shared mem exists
        assert(false);
        return rM2fb;
    }
  }();

  const auto* path = [fbType] {
    switch (fbType) {
      case rM1:
      case Shim:
        return fb_path;

      case rM2fb:

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
    return Error{ "rm2fb not supported" };
  }

  Canvas canvas(memory, width, height, stride, components);
  return FrameBuffer(fbType, fd, canvas);
}

void
FrameBuffer::close() {
  if (canvas.getMemory() != nullptr) {
    munmap(canvas.getMemory(), canvas.totalSize());
  }
  canvas = Canvas{};

  if (fd != -1) {
    ::close(fd);
  }
  fd = -1;
}

void
FrameBuffer::doUpdate(Rect region, Waveform waveform, UpdateFlags flags) const {
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

  assert([&] {
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
    return true;
  }());

  ioctl(fd, MXCFB_SEND_UPDATE, &update);
}

} // namespace rmlib::fb
