#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include <linux/fb.h>
#include <sys/ioctl.h>

#include "print.h"

// Fix info:
//   id: mxs-lcdif
//   smem_start: 0xa9d00000
//   smem_len: 0x2000000
//   type: 0
//   type_aux: 0
//   visual: 0x2
//   xpanstep: 0
//   ypanstep: 0x1
//   ywrapstep: 0x1
//   line_length: 0x410
//   mmio_start: 0
//   mmio_len: 0
//   accel: 0
//   capabilities: 0
//
// Var info:
//   xres: 0x104
//   yres: 0x580
//   xres_virtual: 0x104
//   yres_virtual: 0x5d80
//   xoffset: 0
//   yoffset: 0x1600
//   bits_per_pixel: 0x20
//   grayscale: 0
//   red: { offset: 0x10 length: 0x8 msb_right: 0}
//   green: { offset: 0x8 length: 0x8 msb_right: 0}
//   blue: { offset: 0 length: 0x8 msb_right: 0}
//   transp: { offset: 0 length: 0 msb_right: 0}
//   nonstd: 0
//   activate: 0
//   height: 0
//   width: 0
//   accel_flags: 0
//   pixclock: 0x7080
//   left_margin: 0x1
//   right_margin: 0x1
//   upper_margin: 0x1
//   lower_margin: 0x8f
//   hsync_len: 0x1
//   vsync_len: 0x1
//   sync: 0
//   vmode: 0
//   rotate: 0
//   colorspace: 0

int
main() {
  int fd = open("/dev/fb0", O_RDWR);
  if (fd < 0) {
    perror("open");
    return EXIT_FAILURE;
  }

  fb_fix_screeninfo finfo;
  int res = ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
  if (res != 0) {
    perror("finfo");
    return EXIT_FAILURE;
  }

  std::cout << "Fix info:\n" << std::hex << std::showbase << finfo << "\n";

  fb_var_screeninfo vinfo;
  res = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
  if (res != 0) {
    perror("vinfo");
    return EXIT_FAILURE;
  }
  std::cout << "Var info:\n" << std::hex << std::showbase << vinfo << "\n";

  close(fd);
  return EXIT_SUCCESS;
}
