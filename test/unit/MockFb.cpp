#include "MockFb.h"

#include "init.h" // kFramebufferMemorySize

#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <mutex>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>

namespace {

std::mutex g_mutex;
std::unordered_map<int, swtcon_test::MockFramebuffer::FdState> g_fds;
bool g_redirectFb0 = false;
bool g_failOpen = false;
std::unordered_map<unsigned long, int> g_failIoctlCounts;

} // namespace

extern "C" int __real_open(const char* path, int flags, ...);
extern "C" int __real_ioctl(int fd, unsigned long request, ...);

// Only intercepts the exact literal path init_framebuffer() itself opens -
// everything else (including every other test's own file I/O) passes
// straight through to the real open().
extern "C" int
__wrap_open(const char* path, int flags, ...) {
  va_list ap;
  va_start(ap, flags);
  mode_t mode = 0;
  if (flags & O_CREAT)
    mode = (mode_t)va_arg(ap, int);
  va_end(ap);

  bool redirect;
  bool failOpen;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    redirect = g_redirectFb0;
    failOpen = g_failOpen;
    g_failOpen = false;
  }

  if (redirect && path && strcmp(path, "/dev/fb0") == 0) {
    if (failOpen) {
      errno = ENOENT;
      return -1;
    }
    int fd = memfd_create("mock-fb0", 0);
    if (fd >= 0) {
      // A real /dev/fb0 already has a fixed, mmap-able size - a fresh memfd
      // doesn't, and init_framebuffer() (unlike init_framebuffer_with_fd())
      // never ftruncate()s before mmap'ing it, so this needs to happen here
      // instead or that mmap would fail against an empty file.
      ftruncate(fd, (off_t)kFramebufferMemorySize);
      std::lock_guard<std::mutex> lock(g_mutex);
      g_fds[fd] = {};
    }
    return fd;
  }

  return (flags & O_CREAT) ? __real_open(path, flags, mode) : __real_open(path, flags);
}

extern "C" int
__wrap_ioctl(int fd, unsigned long request, ...) {
  va_list ap;
  va_start(ap, request);

  bool mocked;
  bool failThis = false;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    mocked = g_fds.count(fd) != 0;
    auto it = g_failIoctlCounts.find(request);
    if (it != g_failIoctlCounts.end() && it->second > 0) {
      failThis = true;
      if (--it->second == 0)
        g_failIoctlCounts.erase(it);
    }
  }

  if (!mocked) {
    void* arg = va_arg(ap, void*);
    va_end(ap);
    return __real_ioctl(fd, request, arg);
  }

  if (failThis) {
    // Not consuming the vararg here - its real type varies by request (see
    // FBIOBLANK's own comment below) and va_end doesn't require it anyway.
    va_end(ap);
    errno = EIO;
    return -1;
  }

  switch (request) {
    case FBIOGET_FSCREENINFO: {
      auto* finfo = va_arg(ap, struct fb_fix_screeninfo*);
      memset(finfo, 0, sizeof(*finfo));
      break;
    }
    case FBIOGET_VSCREENINFO: {
      auto* vinfo = va_arg(ap, struct fb_var_screeninfo*);
      memset(vinfo, 0, sizeof(*vinfo));
      break;
    }
    case FBIOPUT_VSCREENINFO: {
      auto* vinfo = va_arg(ap, struct fb_var_screeninfo*);
      std::lock_guard<std::mutex> lock(g_mutex);
      auto& st = g_fds[fd];
      st.lastYoffset = (int32_t)vinfo->yoffset;
      st.panCount++;
      break;
    }
    case FBIOPAN_DISPLAY: {
      auto* vinfo = va_arg(ap, struct fb_var_screeninfo*);
      std::lock_guard<std::mutex> lock(g_mutex);
      auto& st = g_fds[fd];
      st.lastYoffset = (int32_t)vinfo->yoffset;
      st.panCount++;
      break;
    }
    case FBIOBLANK: {
      // FBIOBLANK's argument is the mode value itself, passed directly (not
      // a pointer), unlike every other fb ioctl this mock handles.
      int mode = va_arg(ap, int);
      std::lock_guard<std::mutex> lock(g_mutex);
      auto& st = g_fds[fd];
      if (mode == FB_BLANK_UNBLANK) {
        st.blanked = false;
        st.unblankCount++;
      } else {
        st.blanked = true;
        st.blankCount++;
      }
      break;
    }
    default: {
      // Unknown request - consume the pointer arg defensively, succeed
      // anyway rather than failing an ioctl this mock doesn't know about.
      (void)va_arg(ap, void*);
      break;
    }
  }

  va_end(ap);
  return 0;
}

namespace swtcon_test {

MockFramebuffer::MockFramebuffer() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_redirectFb0 = true;
}

MockFramebuffer::~MockFramebuffer() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_redirectFb0 = false;
  g_fds.clear();
  g_failOpen = false;
  g_failIoctlCounts.clear();
}

void
MockFramebuffer::registerFd(int fd) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_fds[fd] = {};
}

MockFramebuffer::FdState
MockFramebuffer::state(int fd) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_fds.find(fd);
  return it != g_fds.end() ? it->second : FdState{};
}

void
MockFramebuffer::failNextOpen() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_failOpen = true;
}

void
MockFramebuffer::failNextIoctl(unsigned long request, int count) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_failIoctlCounts[request] = count;
}

} // namespace swtcon_test
