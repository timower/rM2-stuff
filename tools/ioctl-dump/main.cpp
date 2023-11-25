#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <unistd.h>

#include <linux/ioctl.h>
#include <linux/limits.h>
#include <mxcfb.h>

#include "print.h"

// openat(AT_FDCWD, "/dev/fb0", O_RDWR)    = 16
// ioctl(16, FBIOGET_FSCREENINFO, 0x8e495c) = 0
// ioctl(16, FBIOGET_VSCREENINFO, 0x8e49a0) = 0
// ioctl(16, FBIOPUT_VSCREENINFO, 0x8e49a0) = 0
// mmap2(NULL, 24893440, PROT_READ|PROT_WRITE, MAP_SHARED, 16, 0) = 0x6e57c000
// ioctl(16, FBIOPUT_VSCREENINFO, 0x8e49a0) = 0
// ioctl(16, FBIOBLANK, 0)                 = 0

namespace {
constexpr int fake_fd = 1337;

constexpr int xres = 0x104;
constexpr int yres = 0x580;
// constexpr int bytes_per_pixel = 4;

bool inXochitl = false;
void* globalMem = nullptr;

int
handleIOCTL(unsigned long request, char* ptr) {
  if (request == FBIOGET_FSCREENINFO) {
    fb_fix_screeninfo* screeninfo = (fb_fix_screeninfo*)ptr;

    strcpy(screeninfo->id, "mxs-lcdif");
    screeninfo->smem_start = 0xa9d00000;
    screeninfo->smem_len = 0x2000000;
    screeninfo->type = 0;
    screeninfo->type_aux = 0;
    screeninfo->visual = 0x2;
    screeninfo->xpanstep = 0;
    screeninfo->ypanstep = 0x1;
    screeninfo->ywrapstep = 0x1;
    screeninfo->line_length = 0x410;
    screeninfo->mmio_start = 0;
    screeninfo->mmio_len = 0;
    screeninfo->accel = 0;
    screeninfo->capabilities = 0;
    return 0;
  }

  if (request == FBIOGET_VSCREENINFO) {
    fb_var_screeninfo* screeninfo = (fb_var_screeninfo*)ptr;
    screeninfo->xres = xres;
    screeninfo->yres = yres;
    screeninfo->xres_virtual = 0x104;
    screeninfo->yres_virtual = 0x5d80;
    screeninfo->xoffset = 0;
    screeninfo->yoffset = 0x1600;
    screeninfo->bits_per_pixel = 0x20;
    screeninfo->grayscale = 0;
    screeninfo->red = { .offset = 0x10, .length = 0x8, .msb_right = 0 };
    screeninfo->green = { .offset = 0x8, .length = 0x8, .msb_right = 0 };
    screeninfo->blue = { .offset = 0, .length = 0x8, .msb_right = 0 };
    screeninfo->transp = { .offset = 0, .length = 0, .msb_right = 0 };
    screeninfo->nonstd = 0;
    screeninfo->activate = 0;
    screeninfo->height = 0;
    screeninfo->width = 0;
    screeninfo->accel_flags = 0;
    screeninfo->pixclock = 0x7080;
    screeninfo->left_margin = 0x1;
    screeninfo->right_margin = 0x1;
    screeninfo->upper_margin = 0x1;
    screeninfo->lower_margin = 0x8f;
    screeninfo->hsync_len = 0x1;
    screeninfo->vsync_len = 0x1;
    screeninfo->sync = 0;
    screeninfo->vmode = 0;
    screeninfo->rotate = 0;
    screeninfo->colorspace = 0;
    return 0;
  }

  if (request == FBIOPUT_VSCREENINFO) {
    std::cout << "Put info:\n"
              << std::hex << std::showbase << *(fb_var_screeninfo*)ptr << "\n";
    return 0;
  }

  if (request == FBIOBLANK) {
    std::cout << "FBIOBLANK: " << (intptr_t)ptr << "\n";
    return 0;
  }

  if (request == FBIOPAN_DISPLAY) {
    auto offset = ((fb_var_screeninfo*)ptr)->yoffset;
    auto idx = offset / yres;
    std::cout << "FBIOPAN: " << std::dec << std::setw(2) << idx << " "
              << std::hex << std::showbase << offset << "\n";
    return 0;
  }

  std::cerr << "UNHANDLED IOCTL" << ' ' << request << std::endl;
  return 0;
}

} // namespace

extern "C" {

int
open64(const char* pathname, int flags, mode_t mode = 0) {
  if (inXochitl && pathname == std::string("/dev/fb0")) {
    return fake_fd;
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open64");

  return func_open(pathname, flags, mode);
}

int
open(const char* pathname, int flags, mode_t mode = 0) {
  if (inXochitl && pathname == std::string("/dev/fb0")) {
    return fake_fd;
  }

  static const auto func_open =
    (int (*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");

  return func_open(pathname, flags, mode);
}

int
close(int fd) {
  if (inXochitl && fd == fake_fd) {
    return 0;
  }

  static const auto func_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
  return func_close(fd);
}

int
ioctl(int fd, unsigned long request, char* ptr) {
  if (inXochitl && fd == fake_fd) {
    return handleIOCTL(request, ptr);
  }

  static auto func_ioctl =
    (int (*)(int, unsigned long request, ...))dlsym(RTLD_NEXT, "ioctl");

  return func_ioctl(fd, request, ptr);
}

void*
mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off) {
  if (inXochitl && fildes == fake_fd) {
    // mmap2(NULL, 24893440, PROT_READ|PROT_WRITE, MAP_SHARED, 16, 0) =
    // 0x6e57c000
    globalMem = malloc(len);
    return globalMem;
  }

  static auto func_mmap = (void* (*)(void* addr,
                                     size_t len,
                                     int prot,
                                     int flags,
                                     int fildes,
                                     off_t off))dlsym(RTLD_NEXT, "mmap");

  return func_mmap(addr, len, prot, flags, fildes, off);
}

int
__libc_start_main(int (*_main)(int, char**, char**),
                  int argc,
                  char** argv,
                  int (*init)(int, char**, char**),
                  void (*fini)(void),
                  void (*rtld_fini)(void),
                  void* stack_end) {

  char pathBuffer[PATH_MAX];
  auto size = readlink("/proc/self/exe", pathBuffer, PATH_MAX);

  if (std::string_view(pathBuffer, size) == "/usr/bin/xochitl") {
    inXochitl = true;
    std::cout << "In xochitl!\n";
  } else {
    std::cout << "Not xochitl!\n";
  }

  auto* func_main =
    (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

  return func_main(_main, argc, argv, init, fini, rtld_fini, stack_end);
};
}
