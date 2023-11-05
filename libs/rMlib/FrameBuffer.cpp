#include "FrameBuffer.h"
#include "Device.h"

#include <unistdpp/file.h>
#include <unistdpp/ioctl.h>

#include <array>
#include <cassert>
#include <climits>
#include <iostream>

#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// 'linux'
#include "mxcfb.h"
#include "rm2.h"

namespace rmlib::fb {
namespace {
constexpr auto fb_path = "/dev/fb0";
} // namespace

ErrorOr<FrameBuffer::Type>
FrameBuffer::detectType() {
  auto devType = TRY(device::getDeviceType());
  switch (devType) {
    default:
    case device::DeviceType::reMarkable1:
      std::cerr << "rM1 currently untested, please open a github issue if "
                   "you encounter any issues\n";

      return rM1;
    case device::DeviceType::reMarkable2:
      if (getenv("RM2STUFF_RM2FB") != nullptr) {
        std::cerr << "Using our own rm2fb!\n";
        return rM2Stuff;
      }

      if (getenv("RM2FB_SHIM") != nullptr) {
        std::cerr << "Using rm2fb shim\n";
        return Shim;
      }

      // check if shared mem exists
      assert(false);
      return Error::make("Unsupported device, please install rm2fb");
  }
}

ErrorOr<FrameBuffer>
FrameBuffer::open() {
  const auto fbType = TRY(detectType());

  auto fd = TRY(unistdpp::open(fb_path, O_RDWR));

  fb_var_screeninfo screeninfo{};
  TRY(
    unistdpp::ioctl<fb_var_screeninfo*>(fd, FBIOGET_VSCREENINFO, &screeninfo));
  fb_fix_screeninfo fixScreenInfo{};
  TRY(unistdpp::ioctl<fb_fix_screeninfo*>(
    fd, FBIOGET_FSCREENINFO, &fixScreenInfo));

  auto components = int(screeninfo.bits_per_pixel / CHAR_BIT);
  auto width = int(screeninfo.xres);
  auto height = int(screeninfo.yres);
  auto stride = int(fixScreenInfo.line_length);

  auto* memory = static_cast<uint8_t*>(mmap(
    nullptr, stride * height, PROT_READ | PROT_WRITE, MAP_SHARED, fd.fd, 0));

  if (memory == nullptr) {
    return Error::make("Error mapping fb");
  }

  Canvas canvas(memory, width, height, stride, components);
  return FrameBuffer(fbType, std::move(fd), canvas);
}

void
FrameBuffer::close() {
  if (canvas.getMemory() != nullptr && fd.isValid()) {
    munmap(canvas.getMemory(), canvas.totalSize());
  }
  canvas = Canvas{};
}

void
FrameBuffer::doUpdate(Rect region, Waveform waveform, UpdateFlags flags) const {
  auto update = mxcfb_update_data{};

  update.update_region.left = region.topLeft.x;
  update.update_region.top = region.topLeft.y;
  update.update_region.width = region.width();
  update.update_region.height = region.height();

  if (type == rM2Stuff) {
    update.update_mode = RM2_UPDATE_MODE;
    update.flags = static_cast<int>(flags);
    update.waveform_mode = static_cast<int>(waveform);
  } else {
    update.update_mode = (flags & UpdateFlags::FullRefresh) != 0
                           ? UPDATE_MODE_FULL
                           : UPDATE_MODE_PARTIAL;

    update.waveform_mode = [&] {
      switch (waveform) {
        case Waveform::DU:
        case Waveform::A2:
          return WAVEFORM_MODE_DU;
        default:
        case Waveform::GC16:
          return WAVEFORM_MODE_GC16;
        case Waveform::GC16Fast:
          return WAVEFORM_MODE_GL16;
      }
    }();

    constexpr auto temp_use_remarkable_draw = 0x0018;
    constexpr auto epdc_flag_exp1 = 0x270ce20;

    update.update_marker = 0;
    update.dither_mode = epdc_flag_exp1;
    update.temp = temp_use_remarkable_draw;
    update.flags = 0;
  }

  (void)unistdpp::ioctl<mxcfb_update_data*>(fd, MXCFB_SEND_UPDATE, &update);

  // Debug logging.
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
}

} // namespace rmlib::fb
