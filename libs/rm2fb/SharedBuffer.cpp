#include "SharedBuffer.h"

#include <cstdio>
#include <errno.h>
#include <fcntl.h> /* For O_* constants */
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>

SharedFB::SharedFB(const char* path) {
  errno = 0;

  fd = shm_open(path, O_RDWR | O_CREAT, 0755);

  if (fd == -1 && errno == EACCES) {
    fd = shm_open(path, O_RDWR | O_CREAT, 0755);
  }

  if (fd < 0) {
    perror("Can't open shm");
    return;
  }

  ftruncate(fd, fb_size);
  mem = (uint16_t*)mmap(NULL, fb_size, PROT_WRITE, MAP_SHARED, fd, 0);
}

SharedFB::~SharedFB() {
  if (mem != nullptr) {
    munmap(mem, fb_size);
  }
}

SharedFB::SharedFB(SharedFB&& other) : fd(other.fd), mem(other.mem) {
  other.fd = -1;
  other.mem = nullptr;
}

SharedFB&
SharedFB::operator=(SharedFB&& other) {
  // TODO: release
  this->fd = other.fd;
  this->mem = other.mem;

  other.fd = -1;
  other.mem = nullptr;

  return *this;
}
