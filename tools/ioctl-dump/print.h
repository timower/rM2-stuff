#pragma once

#include <linux/fb.h>
#include <ostream>

std::ostream&
operator<<(std::ostream& os, const fb_fix_screeninfo& info) {
  os << "  id = " << info.id << "\n";
  os << "  smem_start = " << info.smem_start << "\n";

  os << "  smem_len = " << info.smem_len << "\n";
  os << "  type = " << info.type << "\n";
  os << "  type_aux = " << info.type_aux << "\n";
  os << "  visual = " << info.visual << "\n";
  os << "  xpanstep = " << info.xpanstep << "\n";
  os << "  ypanstep = " << info.ypanstep << "\n";
  os << "  ywrapstep = " << info.ywrapstep << "\n";
  os << "  line_length = " << info.line_length << "\n";
  os << "  mmio_start = " << info.mmio_start << "\n";

  os << "  mmio_len = " << info.mmio_len << "\n";
  os << "  accel = " << info.accel << "\n";

  os << "  capabilities = " << info.capabilities << "\n";

  return os;
}

std::ostream&
operator<<(std::ostream& os, const fb_bitfield& info) {
  os << "{ .offset = " << info.offset << ", .length = " << info.length
     << ", .msb_right = " << info.msb_right << " }";
  return os;
}

std::ostream&
operator<<(std::ostream& os, const fb_var_screeninfo& info) {
  os << "  xres = " << info.xres << "\n";
  os << "  yres = " << info.yres << "\n";
  os << "  xres_virtual = " << info.xres_virtual << "\n";
  os << "  yres_virtual = " << info.yres_virtual << "\n";
  os << "  xoffset = " << info.xoffset << "\n";
  os << "  yoffset = " << info.yoffset << "\n";
  os << "  bits_per_pixel = " << info.bits_per_pixel << "\n";
  os << "  grayscale = " << info.grayscale << "\n";
  os << "  red = " << info.red << "\n";
  os << "  green = " << info.green << "\n";
  os << "  blue = " << info.blue << "\n";
  os << "  transp = " << info.transp << "\n";
  os << "  nonstd = " << info.nonstd << "\n";
  os << "  activate = " << info.activate << "\n";
  os << "  height = " << info.height << "\n";
  os << "  width = " << info.width << "\n";
  os << "  accel_flags = " << info.accel_flags << "\n";
  os << "  pixclock = " << info.pixclock << "\n";
  os << "  left_margin = " << info.left_margin << "\n";
  os << "  right_margin = " << info.right_margin << "\n";
  os << "  upper_margin = " << info.upper_margin << "\n";
  os << "  lower_margin = " << info.lower_margin << "\n";
  os << "  hsync_len = " << info.hsync_len << "\n";
  os << "  vsync_len = " << info.vsync_len << "\n";
  os << "  sync = " << info.sync << "\n";
  os << "  vmode = " << info.vmode << "\n";
  os << "  rotate = " << info.rotate << "\n";
  os << "  colorspace = " << info.colorspace << "\n";
  return os;
}
