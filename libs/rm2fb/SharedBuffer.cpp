#include "rm2fb/SharedBuffer.h"

#include <unistdpp/file.h>
#include <unistdpp/shared_mem.h>
#include <unistdpp/socket.h>

#include <cstring>
#include <fcntl.h> /* For O_* constants */
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h> /* For mode constants */
#include <sys/un.h>
#include <unistd.h>

using namespace unistdpp;

FbFormat
clampToSupported(FbFormat requested, bool ownSwtcon) {
  if (!ownSwtcon || !requested.isSupported()) {
    return default_fb_format;
  }
  return requested;
}

unistdpp::Result<unistdpp::FD>
allocBlankBuffer(FbFormat format) {
  FD fd{ memfd_create("swtfb", MFD_CLOEXEC) };
  if (!fd.isValid()) {
    return tl::unexpected(getErrno());
  }
  TRY(unistdpp::ftruncate(fd, format.totalSize()));

  // Short-lived mapping just to zero it out - unlike Buffer::mmap(),
  // this one isn't the fixed, currently-active shared address, so it's
  // unmapped again as soon as it goes out of scope.
  auto mem = TRY(unistdpp::mmap(
    nullptr, format.totalSize(), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0));
  memset(mem.get(), UINT8_MAX, format.size());
  memset((char*)mem.get() + format.size(), 0, format.grayscaleSize());

  return fd;
}

uint16_t
rgb32ToRgb565(uint32_t pixel) {
  uint8_t r = (pixel >> 16) & 0xFF;
  uint8_t g = (pixel >> 8) & 0xFF;
  uint8_t b = pixel & 0xFF;
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void
convertToRGB565(const FbFormat& srcFormat, const void* src, uint16_t* dst) {
  const auto* px = static_cast<const uint32_t*>(src);
  const int n = srcFormat.width * srcFormat.height;
  for (int i = 0; i < n; i++) {
    dst[i] = rgb32ToRgb565(px[i]);
  }
}

unistdpp::Result<void>
Buffer::alloc(FbFormat fmt) {
  setFD(TRY(allocBlankBuffer(fmt)), fmt);
  return mmap();
}

unistdpp::Result<void>
Buffer::send(const unistdpp::FD& client) const {
  return unistdpp::sendFDTo(client, fd.fd);
}

unistdpp::Result<void>
Buffer::recv(const unistdpp::FD& client) {
  format = TRY(client.readAll<FbFormat>());
  fd = unistdpp::FD(TRY(unistdpp::recvFD(client)));
  return {};
}

unistdpp::Result<void>
Buffer::mmap() {
  void* addr;
  if (mem) {
    addr = mem.get();
    mem.release();
  } else {
    // Reserve enough virtual address space for the largest format this fd
    // could ever be swapped for (see resume()'s format-changing setFD()
    // calls in ServerInternal.h) before mapping the real content - a plain
    // mmap() sized to just this call's `format` would let the kernel pick
    // an address with only that much room nearby, and a *later* MAP_FIXED
    // remap to a bigger format at that same address would then have to grow
    // into memory it was never guaranteed to own, silently clobbering
    // whatever's actually there. Confirmed: this is exactly what crashed
    // rm2fb_server_swtcon the first time a real 32bpp (RGB32) xochitl
    // client became front on a real OS 3.27 image.
    auto reservation = TRY(unistdpp::mmap(nullptr,
                                          max_fb_format.totalSize(),
                                          PROT_NONE,
                                          MAP_PRIVATE | MAP_ANONYMOUS,
                                          unistdpp::FD{},
                                          0));
    addr = reservation.get();
    reservation.release();
  }

  // Only map the real content over as much of the reservation as this
  // format actually needs - never touch (or claim as backed) whatever's
  // beyond format.totalSize(), since the underlying fd is only ftruncate'd
  // to that size (see allocBlankBuffer()).
  auto real = TRY(unistdpp::mmap(addr,
                                 format.totalSize(),
                                 PROT_WRITE | PROT_READ,
                                 MAP_SHARED | MAP_FIXED,
                                 fd,
                                 0));
  real.release();

  // Track the *full* reservation size in the stored deleter, not just this
  // call's (possibly smaller) format - munmap() cleanly unmaps whatever mix
  // of real-content and still-PROT_NONE pages the range actually holds, so
  // this never leaks the unused tail of the reservation.
  mem = unistdpp::MmapPtr(
    addr, unistdpp::MmapDeleter{ (size_t)max_fb_format.totalSize() });

  return {};
}

Buffer&
getGlobalFrameBuffer() {
  static Buffer instance;
  return instance;
}
