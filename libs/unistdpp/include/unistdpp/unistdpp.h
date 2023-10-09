#pragma once

#include "error.h"
#include "traits.h"

#include <unistd.h>

#include <utility>

namespace unistdpp {

/// Managed file descriptor. Prevents copy, closes on destruction.
struct FD {
  static constexpr int invalid_fd = -1;

  // Use ENODATA as EOF
  static constexpr std::errc eof_error = std::errc::no_message_available;

  int fd = invalid_fd;

  explicit FD(int fd = invalid_fd) noexcept : fd(fd) {}
  FD(FD&& other) noexcept : fd(other.fd) { other.fd = invalid_fd; }

  FD& operator=(FD&& other) noexcept {
    std::swap(other.fd, fd);
    other.close();
    return *this;
  }

  ~FD() noexcept { close(); }

  void close() noexcept {
    if (fd >= 0) {
      ::close(fd);
    }
    fd = invalid_fd;
  }

  FD(const FD&) = delete;
  FD& operator=(const FD&) = delete;

  [[nodiscard]] bool isValid() const noexcept { return fd >= 0; }

  explicit operator bool() const noexcept { return isValid(); }

  [[nodiscard]] Result<void> readAll(void* buf, std::size_t size) const;

  template<typename T,
           typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  [[nodiscard]] Result<T> readAll() const {
    T result;
    return readAll(&result, sizeof(T)).map([&result] {
      return std::move(result);
    });
  }

  [[nodiscard]] Result<void> writeAll(const void* buf, std::size_t) const;

  template<typename T,
           typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  [[nodiscard]] Result<void> writeAll(const T& t) const {
    return writeAll(&t, sizeof(T));
  }
};

template<>
struct WrapperTraits<const FD&> {
  static int arg(const FD& fd) { return fd.fd; }
};

template<>
struct WrapperTraits<Result<FD>> {
  static Result<FD> ret(int res) {
    if (res == -1) {
      return tl::unexpected(getErrno());
    }
    return FD(res);
  }
};

} // namespace unistdpp
