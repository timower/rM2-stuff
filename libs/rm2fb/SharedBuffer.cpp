#include "SharedBuffer.h"

#include <unistdpp/file.h>
#include <unistdpp/shared_mem.h>

#include <cstring>
#include <fcntl.h> /* For O_* constants */
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>

using namespace unistdpp;

namespace {
constexpr mode_t shm_mode = 0755;
}

Result<SharedFB>
SharedFB::open(const char* path) {
  auto fd = unistdpp::shm_open(path, O_RDWR, shm_mode);

  bool clear = false;
  if (!fd.has_value() && fd.error() == std::errc::no_such_file_or_directory) {
    fd = unistdpp::shm_open(path, O_RDWR | O_CREAT, shm_mode);
    clear = true;
  }

  if (!fd.has_value() && fd.error() == std::errc::permission_denied) {
    fd = unistdpp::shm_open(path, O_RDWR | O_CREAT, shm_mode);
  }

  if (!fd.has_value()) {
    return tl::unexpected(fd.error());
  }

  TRY(unistdpp::ftruncate(*fd, total_size));
  auto mem =
    TRY(unistdpp::mmap(nullptr, total_size, PROT_WRITE, MAP_SHARED, *fd, 0));
  if (clear) {
    memset(mem.get(), UINT8_MAX, fb_size);
    memset((char*)mem.get() + fb_size, 0, grayscale_size);
  }

  return SharedFB{ .fd = std::move(*fd), .mem = std::move(mem) };
}

const Result<SharedFB>&
SharedFB::getInstance() {
  static auto instance = SharedFB::open(default_fb_name);
  return instance;
}
