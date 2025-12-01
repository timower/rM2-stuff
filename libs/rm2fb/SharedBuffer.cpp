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

// namespace {
// constexpr mode_t shm_mode = 0666;
// }
//
// Result<SharedFB>
// SharedFB::open(const char* path) {
//   auto fd = unistdpp::shm_open(path, O_RDWR, 0);
//
//   bool clear = false;
//   if (!fd.has_value() && (fd.error() == std::errc::no_such_file_or_directory
//   ||
//                           fd.error() == std::errc::permission_denied)) {
//     int origMask = umask(0);
//     fd = unistdpp::shm_open(path, O_RDWR | O_CREAT, shm_mode);
//     umask(origMask);
//
//     clear = true;
//   }
//
//   if (!fd.has_value()) {
//     return tl::unexpected(fd.error());
//   }
//
//   TRY(unistdpp::ftruncate(*fd, total_size));
//   auto mem =
//     TRY(unistdpp::mmap(nullptr, total_size, PROT_WRITE, MAP_SHARED, *fd, 0));
//   if (clear) {
//     memset(mem.get(), UINT8_MAX, fb_size);
//     memset((char*)mem.get() + fb_size, 0, grayscale_size);
//   }
//   return SharedFB{ .fd = std::move(*fd), .mem = std::move(mem) };
// }
//

unistdpp::Result<void>
SharedFB::alloc() {
  fd = unistdpp::FD(memfd_create("swtfb", MFD_CLOEXEC));
  if (!fd.isValid()) {
    return tl::unexpected(getErrno());
  }

  TRY(unistdpp::ftruncate(fd, total_size));

  mem = TRY(unistdpp::mmap(
    nullptr, total_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0));

  memset(mem.get(), UINT8_MAX, fb_size);
  memset((char*)mem.get() + fb_size, 0, grayscale_size);

  return {};
}

unistdpp::Result<void>
SharedFB::send(const unistdpp::FD& client) const {
  struct msghdr msg = {};

  // Allocate space for the control message
  char controlBuf[CMSG_SPACE(sizeof(int))];

  // A mandatory but mostly irrelevant payload byte
  char data = '\01';
  iovec iov[1];
  iov[0].iov_base = &data;
  iov[0].iov_len = sizeof(data);

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = controlBuf;
  msg.msg_controllen = sizeof(controlBuf);

  // Get a pointer to the first control message header
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg == nullptr) {
    return tl::unexpected(std::errc::not_enough_memory);
  }

  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS; // Indicates we are sending file descriptors
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));

  // Put the file descriptor into the control message data
  *((int*)CMSG_DATA(cmsg)) = fd.fd;

  TRY(unistdpp::sendmsg(client, &msg, 0));
  return {};
}

unistdpp::Result<void>
SharedFB::recv(const unistdpp::FD& client) {
  struct msghdr msg = {};
  // Allocate space for the control message
  char controlBuf[CMSG_SPACE(sizeof(int))];

  // A mandatory but mostly irrelevant payload byte
  char data;
  iovec iov[1];
  iov[0].iov_base = &data;
  iov[0].iov_len = sizeof(data);

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = controlBuf;
  msg.msg_controllen = sizeof(controlBuf);

  TRY(unistdpp::recvmsg(client, &msg, 0));

  // Iterate through control messages
  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg == nullptr || cmsg->cmsg_level != SOL_SOCKET ||
      cmsg->cmsg_type != SCM_RIGHTS) {
    return tl::unexpected(std::errc::bad_message);
  }

  fd = unistdpp::FD(*((int*)CMSG_DATA(cmsg)));
  TRY(unistdpp::fcntl<int>(fd, F_SETFD, FD_CLOEXEC));

  return {};
}

unistdpp::Result<void>
SharedFB::mmap() {
  mem = TRY(unistdpp::mmap(
    nullptr, total_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0));
  return {};
}

SharedFB&
SharedFB::getInstance() {
  static SharedFB instance;
  return instance;
}
