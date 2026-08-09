#pragma once

#include <cstdint>

namespace swtcon_test {

// A realistic fake /dev/fb0 + ioctl responder, active for the lifetime of
// the object. Implemented via `-Wl,--wrap=open,--wrap=ioctl` (see MockFb.cpp)
// rather than a real device or LD_PRELOAD, so it works in-process against
// this same test binary.
//
// While an instance is alive:
//  - open("/dev/fb0", ...) never touches a real device - it's redirected to
//    a private memfd, letting init_framebuffer()'s own real path (normally
//    untestable without a real panel) run end-to-end.
//  - FBIOGET_FSCREENINFO/FBIOGET_VSCREENINFO/FBIOPUT_VSCREENINFO/
//    FBIOPAN_DISPLAY/FBIOBLANK against that fd, or any fd explicitly handed
//    to registerFd() (e.g. SwtconFixture's own memfd, passed via
//    InitParams::framebufferFd), succeed the way a real panel driver would -
//    unlike a bare, unregistered memfd, where every one of those ioctls
//    fails outright (ENOTTY). This is what actually lets
//    pan_and_unblank/blank_fb/prime_display's *success* branches run, and
//    so lets worker_thread_func/display_thread_func be driven through their
//    real hardware-touching steps instead of just their skeletons.
// Every other fd/path is passed straight through to the real open()/ioctl().
//
// Only one instance may be alive at a time (tests run sequentially) - not
// thread-safe to construct/destruct concurrently, though the mocked
// ioctl()/open() calls themselves are safe to make from any thread (needed
// since worker_thread_func/display_thread_func run on their own threads).
class MockFramebuffer {
public:
  MockFramebuffer();
  ~MockFramebuffer();
  MockFramebuffer(const MockFramebuffer&) = delete;
  MockFramebuffer& operator=(const MockFramebuffer&) = delete;

  // Lets ioctl()s against an already-open fd succeed too, without going
  // through the /dev/fb0 open() redirection - e.g. SwtconFixture's own
  // memfd/temp-file fd (InitParams::framebufferFd), which init_framebuffer_
  // with_fd() never issues any ioctl against itself but pan_and_unblank/
  // blank_fb/pan_to_frame later do.
  static void registerFd(int fd);

  // Inspectable per-fd state, for asserting the mock actually observed
  // realistic pan/blank activity.
  struct FdState {
    bool blanked = true;
    int panCount = 0;
    int unblankCount = 0;
    int blankCount = 0;
    int32_t lastYoffset = 0;
  };
  static FdState state(int fd);

  // Failure injection, for exercising init_framebuffer()'s own error
  // branches deterministically - without ever touching a real device
  // either way, unlike simply not installing this mock at all (which would
  // fall through to whatever /dev/fb0 actually is, or isn't, on this host).
  //
  // Makes the next open("/dev/fb0", ...) fail (-1, ENOENT) instead of being
  // redirected, once, then reverts to the normal redirect behavior.
  static void failNextOpen();
  // Makes the next `count` ioctl() calls for `request` (against any mocked
  // fd) fail (-1, EIO) instead of succeeding, then reverts to normal.
  static void failNextIoctl(unsigned long request, int count = 1);
};

} // namespace swtcon_test
