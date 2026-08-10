#pragma once

#include <cstdint>

#include <unistdpp/mmap.h>
#include <unistdpp/unistdpp.h>

constexpr int fb_width = 1404;
constexpr int fb_height = 1872;
constexpr int fb_pixel_size = sizeof(uint16_t);
constexpr int fb_size = fb_width * fb_height * fb_pixel_size;

constexpr int grayscale_size = fb_width * fb_height;
constexpr int total_size = fb_size + grayscale_size;

// Pixel layout of a client's shared color-plane buffer - RGB565 is every
// OS version before 3.27; RGB32 is what 3.27+ xochitl switched to (see
// doc/swtcon_3.27_diff.md). Explicit rather than inferring from byte
// count, since converting between formats needs to know the real layout.
enum class PixelFormat : int32_t { RGB565, RGB32 };

// Describes a client's shared framebuffer geometry/layout - width/height
// track the physical panel resolution (unlikely to vary in practice) and
// pixelFormat tracks the OS-version-dependent color-plane layout. The
// grayscale plane is always 1 byte/pixel regardless of pixelFormat.
struct FbFormat {
  int32_t width = fb_width;
  int32_t height = fb_height;
  PixelFormat pixelFormat = PixelFormat::RGB565;

  int32_t pixelSize() const {
    return pixelFormat == PixelFormat::RGB32 ? 4 : 2;
  }
  int32_t size() const { return width * height * pixelSize(); }
  int32_t grayscaleSize() const { return width * height; }
  int32_t totalSize() const { return size() + grayscaleSize(); }

  // Whether this is one of the geometry/layout combinations the rest of
  // the system actually handles - fixed panel resolution, and a
  // recognized PixelFormat (an unknown enum value would silently fall
  // through pixelSize()'s `== RGB32 ? 4 : 2` as if it were RGB565, while
  // still tripping resume()'s `!= RGB565` conversion path as if it were
  // RGB32 - a real size mismatch, not just a cosmetic one). A client-
  // requested format that fails this must be clamped before it's
  // allocated, stored, or converted - see clampToSupported().
  bool isSupported() const {
    return width == fb_width && height == fb_height &&
           (pixelFormat == PixelFormat::RGB565 ||
            pixelFormat == PixelFormat::RGB32);
  }

  // Written out manually (not `= default`) - some targets in this
  // codebase still build as C++17, and rewritten `!=` candidates from a
  // defaulted `==` are a C++20 feature.
  bool operator==(const FbFormat& other) const {
    return width == other.width && height == other.height &&
           pixelFormat == other.pixelFormat;
  }
  bool operator!=(const FbFormat& other) const { return !(*this == other); }
};
constexpr FbFormat default_fb_format{};

// The largest format any client can currently request (see readUnixSock()'s
// clamp in ServerInternal.h) - SharedFB::mmap() reserves this much virtual
// address space up front so a later swap to a bigger format at the same
// fixed address never has to grow into memory it doesn't own. Update this
// if PixelFormat ever grows a layout wider than RGB32.
constexpr FbFormat max_fb_format{ .pixelFormat = PixelFormat::RGB32 };

// Clamps a client's Init-requested format to one the rest of the system
// can actually handle: only a client driving its own independent swtcon
// (ownSwtcon) may keep a non-default format, and even then only if it's
// isSupported() - everything else (a regular client, or an unsupported
// request) falls back to default_fb_format.
FbFormat
clampToSupported(FbFormat requested, bool ownSwtcon);

// Allocates a fresh, blank buffer with the same size/layout as a SharedFB
// of the given format, but not tied to a SharedFB instance and not left
// mapped - for handing out to a client that isn't (yet, or ever) the
// currently-active one, see UnixClient::buffer in Server.cpp.
unistdpp::Result<unistdpp::FD>
allocBlankBuffer(FbFormat format = default_fb_format);

// Converts one RGB32 (xRGB8888) pixel into RGB565 - a plain scalar
// conversion (no gamma table/NEON), only meant for previewing/streaming a
// non-default-format client's real content through paths that only ever
// understood RGB565 - this server's own swtcon (see
// doc/swtcon_3.27_diff.md) and the TCP debug/screenshot protocol
// (doTCPUpdate, Server.cpp), neither of which is worth teaching a second
// pixel format.
uint16_t
rgb32ToRgb565(uint32_t pixel);

// Converts one RGB32 (xRGB8888) pixel plane into RGB565, pixel by pixel via
// rgb32ToRgb565().
void
convertToRGB565(const FbFormat& srcFormat, const void* src, uint16_t* dst);

struct SharedFB {
  bool isValid() const { return fd.isValid(); }
  int getFd() const { return fd.fd; }
  // Format travels with the fd so mmap() always knows how many bytes the
  // currently-set fd actually holds - keeping them in separate calls
  // risks mmap() using stale sizing for a freshly-swapped-in fd.
  void setFD(unistdpp::FD fd, FbFormat fmt = default_fb_format) {
    format = fmt;
    this->fd = std::move(fd);
  }
  unistdpp::FD takeFD() { return std::move(fd); }

  FbFormat getFormat() const { return format; }

  void* getFb() const { return mem.get(); }
  void* getGrayBuffer() const { return ((char*)mem.get()) + format.size(); }

  // Reads the granted format (written by the server right before the fd -
  // see readUnixSock()'s Init reply) off the wire itself, rather than
  // taking it as an argument - keeps the two reads (format, then fd) that
  // must always happen together in one place instead of duplicated at
  // every call site.
  unistdpp::Result<void> recv(const unistdpp::FD& client);
  unistdpp::Result<void> mmap();

  unistdpp::Result<void> alloc(FbFormat fmt = default_fb_format);
  unistdpp::Result<void> send(const unistdpp::FD& client) const;

  static SharedFB& getInstance();

private:
  unistdpp::FD fd;
  unistdpp::MmapPtr mem;
  FbFormat format;
};
