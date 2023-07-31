#pragma once

#include <cstdint>

constexpr int fb_width = 1404;
constexpr int fb_height = 1872;
constexpr int fb_pixel_size = sizeof(uint16_t);
constexpr int fb_size = fb_width * fb_height * fb_pixel_size;

#define DEFAULT_FB_NAME "/rm2fb.01"

struct SharedFB {
  int fd = -1;
  uint16_t* mem = nullptr;

  SharedFB(const char* path);
  ~SharedFB();

  SharedFB(const SharedFB& other) = delete;
  SharedFB& operator=(const SharedFB& other) = delete;

  SharedFB(SharedFB&& other);
  SharedFB& operator=(SharedFB&& other);
};
