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

struct SharedFB {
  bool isValid() const { return fd.isValid(); }
  int getFd() const { return fd.fd; }
  void setFD(unistdpp::FD fd) { this->fd = std::move(fd); }
  unistdpp::FD takeFD() { return std::move(fd); }

  void* getFb() const { return mem.get(); }
  void* getGrayBuffer() const { return ((char*)mem.get()) + fb_size; }

  unistdpp::Result<void> recv(const unistdpp::FD& client);
  unistdpp::Result<void> mmap();

  unistdpp::Result<void> alloc();
  unistdpp::Result<void> send(const unistdpp::FD& client) const;

  static SharedFB& getInstance();

private:
  unistdpp::FD fd;
  unistdpp::MmapPtr mem;
};
