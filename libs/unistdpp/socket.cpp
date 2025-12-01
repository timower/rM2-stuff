#include "unistdpp/socket.h"
#include "unistdpp/file.h"

namespace unistdpp {
Address
Address::fromUnixPath(const char* path) {
  Address res;
  auto* addr = new (&res.storage) sockaddr_un(); // NOLINT
  memset(addr, 0, sizeof(sockaddr_un));
  addr->sun_family = AF_UNIX;

  if (path != nullptr) {
    strncpy(&addr->sun_path[0], path, sizeof(addr->sun_path) - 1);
    res.addressSize = sizeof(sockaddr_un);
  } else {
    res.addressSize = sizeof(sa_family_t);
  }

  return res;
}

Address
Address::fromHostPort(int32_t host, int port) { // NOLINT
  Address res;
  res.addressSize = sizeof(sockaddr_in);
  auto* addr = new (&res.storage) sockaddr_in(); // NOLINT
  memset(addr, 0, sizeof(sockaddr_in));
  addr->sin_family = AF_INET;

  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(host);

  return res;
}

Result<Address>
Address::fromHostPort(std::string_view host, int port) {
  Address res;
  res.addressSize = sizeof(sockaddr_in);
  auto* addr = new (&res.storage) sockaddr_in(); // NOLINT
  memset(addr, 0, sizeof(sockaddr_in));
  addr->sin_family = AF_INET;

  addr->sin_port = htons(port);

  int err = inet_pton(AF_INET, host.data(), &addr->sin_addr);
  if (err == 0) {
    return tl::unexpected(std::errc::invalid_argument);
  }
  if (err < 0) {
    return tl::unexpected(getErrno());
  }

  return res;
}

unistdpp::Result<void>
sendFDTo(const FD& sock, int fd, Address* addr, char data) {
  struct msghdr msg = {};

  // Allocate space for the control message
  char controlBuf[CMSG_SPACE(sizeof(int))];

  // A mandatory but mostly irrelevant payload byte
  iovec iov[1];
  iov[0].iov_base = &data;
  iov[0].iov_len = sizeof(data);

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = controlBuf;
  msg.msg_controllen = sizeof(controlBuf);

  if (addr != nullptr) {
    msg.msg_name = addr->ptr();
    msg.msg_namelen = addr->size();
  }

  // Get a pointer to the first control message header
  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg == nullptr) {
    return tl::unexpected(std::errc::not_enough_memory);
  }

  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS; // Indicates we are sending file descriptors
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));

  // Put the file descriptor into the control message data
  *((int*)CMSG_DATA(cmsg)) = fd;

  TRY(unistdpp::sendmsg(sock, &msg, 0));
  return {};
}

unistdpp::Result<int>
recvFD(const FD& sock) {
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

  TRY(unistdpp::recvmsg(sock, &msg, 0));

  // Iterate through control messages
  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg == nullptr || cmsg->cmsg_level != SOL_SOCKET ||
      cmsg->cmsg_type != SCM_RIGHTS) {
    return tl::unexpected(std::errc::bad_message);
  }

  int res = *((int*)CMSG_DATA(cmsg));
  if (::fcntl(res, F_SETFD, FD_CLOEXEC) == -1) {
    return tl::unexpected(getErrno());
  }

  return res;
}

} // namespace unistdpp
