#pragma once

#include <catch2/catch_test_macros.hpp>

#include "init.h"
#include "swtcon.h"
#include "update.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <unistd.h>

#ifdef __linux__
#include <sys/mman.h>
#endif

namespace swtcon_test {

// Runs `fn` on a separate thread and waits up to `timeout` for it to
// finish. Returns false (leaking the still-running thread) on timeout
// instead of hanging the whole test binary - only meant for asserting a
// call completes promptly (e.g. swtcon_shutdown after a real thread test).
template<typename Fn>
inline bool
run_with_timeout(Fn fn, std::chrono::milliseconds timeout) {
  auto done = std::make_shared<std::atomic<bool>>(false);
  std::thread t([fn, done] {
    fn();
    *done = true;
  });
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!*done && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  if (*done) {
    t.join();
    return true;
  }
  t.detach();
  return false;
}

// Anonymous, already-unlinked fd: memfd_create() on Linux, a mkstemp'd temp
// file elsewhere (e.g. a macOS dev-host build).
inline int
make_anon_fd() {
#ifdef __linux__
  return memfd_create("swtcon-test-fb", 0);
#else
  char path[] = "/tmp/swtcon-test-fb-XXXXXX";
  int fd = mkstemp(path);
  if (fd >= 0) unlink(path);
  return fd;
#endif
}

// A real waveform file pulled off RemEmu (/usr/share/remarkable/*.wbf),
// checked in xz-compressed and extracted at build time - see
// test/unit/assets/test.wbf.tar.xz and this target's CMakeLists.txt.
// Using the real thing instead of a synthetic one exercises load_waveform's
// actual RLE/packing code and gives every mode real, multi-phase LUTs.
inline std::filesystem::path
test_wbf_path() {
  return std::filesystem::path(SWTCON_TEST_WBF_DIR) / "test.wbf";
}

// RAII wrapper around swtcon_init()/swtcon_shutdown() against a fake
// framebuffer fd and the real test.wbf - no real hardware touched.
class SwtconFixture {
public:
  explicit SwtconFixture(bool startThreads = false) {
    fd = make_anon_fd();
    REQUIRE(fd >= 0);

    wbfPath = test_wbf_path().string();
    InitParams params;
    params.skipPidLock = true;
    params.waveformPathOverride = wbfPath.c_str();
    params.framebufferFd = fd;
    params.startThreads = startThreads;
    image = swtcon_init(params);
    REQUIRE(image != nullptr);
  }

  ~SwtconFixture() { swtcon_shutdown(0); }

  SwtconFixture(const SwtconFixture&) = delete;
  SwtconFixture& operator=(const SwtconFixture&) = delete;

  std::string wbfPath;
  int fd = -1;
  uint16_t* image = nullptr;
};

} // namespace swtcon_test
