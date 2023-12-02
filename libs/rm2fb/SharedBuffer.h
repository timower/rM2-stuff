#pragma once

#include <cstdint>

#include <unistdpp/mmap.h>
#include <unistdpp/unistdpp.h>

constexpr int fb_width = 1404;
constexpr int fb_height = 1872;
constexpr int fb_pixel_size = sizeof(uint16_t);
constexpr int fb_size = fb_width * fb_height * fb_pixel_size;

constexpr auto default_fb_name = "/rm2fb.01";

struct SharedFB {
  unistdpp::FD fd;
  unistdpp::MmapPtr mem;

  static unistdpp::Result<SharedFB> open(const char* path);

  static const unistdpp::Result<SharedFB>& getInstance();
};
