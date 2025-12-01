#include "rm2fb/SharedBuffer.h"

#include <unistdpp/file.h>
#include <unistdpp/shared_mem.h>
#include <unistdpp/socket.h>

#include <cstring>
#include <fcntl.h> /* For O_* constants */
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h> /* For mode constants */
#include <sys/un.h>
#include <unistd.h>

using namespace unistdpp;

unistdpp::Result<void>
SharedFB::alloc() {
  fd = unistdpp::FD(memfd_create("swtfb", MFD_CLOEXEC));
  if (!fd.isValid()) {
    return tl::unexpected(getErrno());
  }
  TRY(unistdpp::ftruncate(fd, total_size));

  TRY(mmap());

  memset(mem.get(), UINT8_MAX, fb_size);
  memset((char*)mem.get() + fb_size, 0, grayscale_size);

  return {};
}

unistdpp::Result<void>
SharedFB::send(const unistdpp::FD& client) const {
  return unistdpp::sendFDTo(client, fd.fd);
}

unistdpp::Result<void>
SharedFB::recv(const unistdpp::FD& client) {
  fd = unistdpp::FD(TRY(unistdpp::recvFD(client)));
  return {};
}

unistdpp::Result<void>
SharedFB::mmap() {
  if (mem) {
    auto* addr = mem.get();
    mem.release();
    mem = TRY(unistdpp::mmap(
      addr, total_size, PROT_WRITE | PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0));
  } else {
    mem = TRY(unistdpp::mmap(
      nullptr, total_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0));
  }

  return {};
}

SharedFB&
SharedFB::getInstance() {
  static SharedFB instance;
  return instance;
}
