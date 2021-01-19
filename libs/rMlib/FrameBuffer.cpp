#include "FrameBuffer.h"
#include "Device.h"

#include <sys/ioctl.h>

#include "mxcfb.h"

#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace rmlib::fb {
namespace {
constexpr auto shm_path = "/dev/shm/swtfb.01";
}

std::optional<FrameBuffer>
FrameBuffer::open() {
  auto devType = device::getDeviceType();
  if (!devType.has_value()) {
    return std::nullopt;
  }

  const auto fbType = [devType] {
    switch (*devType) {
      case device::DeviceType::reMarkable1:
        std::cerr << "rM1 currently unsupported, please open a github issue\n";
        return rM1;
      case device::DeviceType::reMarkable2:
        if (getenv("RM2FB_SHIM") != nullptr) {
          std::cerr << "Using rm2fb shim\n";
          return Shim;
        }

        // check if shared mem exists
        if (access(shm_path, F_OK) == 0) {
          std::cerr << "Using rm2fb ipc\n";
          return rM2fb;
        }

        // start swtcon
        return Swtcon;
      default:
        assert(false);
    }
  }();

  if (fbType == Swtcon) {
    std::cerr << "No rm2fb found, swtcon not implemented yet\n";
    return std::nullopt;
  }

  const auto* path = [fbType] {
    switch (fbType) {
      case rM1:
      case Shim:
        return "/dev/fb0";

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
    std::cerr << "Error opening " << path << std::endl;
    perror("error");
    return std::nullopt;
  }

  Canvas canvas;
  canvas.width = 1404; // TODO: use ioctl?
  canvas.height = 1872;
  canvas.components = sizeof(uint16_t);
  canvas.memory = static_cast<uint8_t*>(mmap(
    nullptr, canvas.totalSize(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

  if (canvas.memory == nullptr) {
    std::cerr << "Error mapping fb\n";
    return std::nullopt;
  }

  return FrameBuffer(fbType, fd, canvas);
}

void
FrameBuffer::close() {
  if (canvas.memory != nullptr) {
    munmap(canvas.memory, canvas.totalSize());
  }
  canvas.memory = nullptr;

  if (fd != -1) {
    ::close(fd);
  }
  fd = -1;
}

FrameBuffer::~FrameBuffer() {
  close();
}

void
FrameBuffer::doUpdate(Rect region, Waveform waveform, UpdateFlags flags) const {

  switch (type) {
    case rM1:
    case Shim: {
      auto update = mxcfb_update_data{};
      update.waveform_mode = static_cast<int>(waveform);
      update.update_mode = (flags & UpdateFlags::FullRefresh) != 0 ? 1 : 0;
      update.update_region.top = region.topLeft.y;
      update.update_region.left = region.topLeft.x;
      update.update_region.width = region.width();
      update.update_region.height = region.height();

      ioctl(fd, MXCFB_SEND_UPDATE, &update);
    } break;
    case rM2fb:
    case Swtcon:
      // TODO: implement this
      break;
  }
}

} // namespace rmlib::fb
