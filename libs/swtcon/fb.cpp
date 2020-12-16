#include "fb.h"

#include "Addresses.h"
#include "Constants.h"
#include "swtcon.h"

#include <iostream>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace swtcon::fb {

void
orInRange(uint32_t* line, int value, int start, int length) {
  for (int i = start; i < start + length; i++) {
    line[i] |= value;
  }
}

void
fillFirstLine(uint32_t* line) {
  for (unsigned int i = 0; i < pan_line_size / sizeof(uint32_t); i++) {
    line[i] = 0x430000;
  }

  orInRange(line, 0x40000, 20, 123);

  for (int i = 40; i < 103; i++) {
    line[i] &= 0xfffdffff;
  }
}

void
fillLine(uint32_t* line, int value) {
  for (unsigned int i = 0; i < pan_line_size / sizeof(uint32_t); i++) {
    line[i] = 0x410000;
  }
  orInRange(line, 0x200000, 8, 0xb);
  orInRange(line, 0x20000, 0x37, 200);

  if (value < 0) {
    return;
  }

  orInRange(line, 0x100000, 0x1a, 0xea);
  orInRange(line, value & 0xffff, 0x1a, 0xea);
}

void
fillPanBuffer(uint8_t* buffer, int value) {
  // preamble
  fillFirstLine((uint32_t*)buffer);
  fillLine((uint32_t*)(buffer + pan_line_size), -1);
  fillLine((uint32_t*)(buffer + 2 * pan_line_size), -1);

  // actual content
  fillLine((uint32_t*)(buffer + 3 * pan_line_size), value);

  for (int line = 0; line < SCREEN_WIDTH; line++) {
    auto* dest =
      buffer + 4 * pan_line_size + line * pan_line_size; // skip preamble
    memcpy(dest, buffer + 3 * pan_line_size, pan_line_size);
  }
}

int
unblank(int pan) {
  if (*isBlanked == 0) {
    return 1;
  }

  // This does not actually pan??
  fb_var_info->yoffset = pan * pan_buffer_size;
  if (ioctl(*fb_fd, /* set var info */ 0x4601, fb_var_info) == -1) {
    perror("Error setting pan offset (unblank)");
    return 1;
  }

  bool succeeded = false;
  for (int i = 0; i < 5; i++) {
    if (ioctl(*fb_fd, /* unblank */ 0x4611, 0) != -1) {
      succeeded = true;
      break;
    }
    perror("Unable to unblank");
  }
  if (!succeeded) {
    return 1;
  }

  *isBlanked = 0;

  return 0;
}

int
pan(int pan) {
  fb_var_info->yoffset = pan * pan_buffer_size;
  if (ioctl(*fb_fd, /* pan */ 0x4606, fb_var_info) == -1) {
    std::cerr << "arg: " << pan << " offset: " << std::hex
              << fb_var_info->yoffset << std::dec << std::endl;
    perror("Error setting pan offset");
    return 1;
  }

  timespec time;
  clock_gettime(CLOCK_MONOTONIC_RAW, &time);

  pthread_mutex_lock(lastPanMutex);
  *lastPanSec = time.tv_sec;
  *lastPanNsec = time.tv_nsec;
  pthread_mutex_unlock(lastPanMutex);

  return 0;
}

int
blank() {
  *isBlanked = 1;

  if (ioctl(*fb_fd, 0x4611, 3) == -1) {
    perror("Unable to blank");
    return 1;
  }

  return 0;
}

int
openFb(const char* path, int panBuffers) {
  auto panCount = panBuffers + 1;

  auto fd = open(path, O_RDWR);
  *fb_fd = fd;

  std::cout << "Opened" << std::endl;
  if (fd == -1) {
    perror("Error opening fd");
    return -1;
  }

  // TODO: is ioctl fixed needed if it's unused? Probably used for asserts in
  // debug builds?
  fb::fb_fix_screeninfo fix_info;
  if (ioctl(fd, 0x4602, &fix_info) == -1) {
    perror("Unable to get fix info");
    close(fd);
    return -1;
  }

  if (ioctl(fd, 0x4600, fb_var_info) == -1) {
    perror("Unable to get fb var info");
    close(fd);
    return -1;
  }
  std::cout << "got var info" << std::endl;

  fb_var_info->yres = pan_buffer_size;
  fb_var_info->yres_virtual = panCount * pan_buffer_size;
  fb_var_info->yoffset = pan_buffer_size * pan_buffers_count;

  fb_var_info->xres = pan_line_size / 4;
  fb_var_info->xres_virtual = pan_line_size / 4;
  fb_var_info->xoffset = 0;

  fb_var_info->pixclock = 0x7080;

  fb_var_info->lower_margin = 143;
  fb_var_info->upper_margin = 1;
  fb_var_info->left_margin = 1;
  fb_var_info->right_margin = 1;

  fb_var_info->hsync_len = 1;
  fb_var_info->vsync_len = 1;

  fb_var_info->bits_per_pixel = 32;

  if (ioctl(fd, 0x4601, fb_var_info) == -1) {
    perror("Error setting fb var info");
    close(fd);
    return -1;
  }
  std::cout << "set var info" << std::endl;

  uint8_t* ptr = (uint8_t*)mmap(nullptr,
                                panCount * pan_buffer_size * pan_line_size,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd,
                                /* offset */ 0);
  *fb_map_ptr = ptr;
  if (ptr == (void*)-1) {
    perror("Error can't mmap fb");
    close(fd);
    return -1;
  }
  std::cout << "mmaped" << std::endl;

  for (int i = 0; i < panCount; i++) {
    memcpy(ptr + i * pan_buffer_size * pan_line_size,
           zeroBuffer,
           pan_buffer_size * pan_line_size);
  }
  std::cout << "done" << std::endl;

  return 0;
}

void
unmap() {
  munmap(*fb_map_ptr,
         pan_line_size * pan_buffer_size * (pan_buffers_count + 1));
  close(*fb_fd);
}
} // namespace swtcon::fb
