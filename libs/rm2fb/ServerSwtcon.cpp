#include "Server.h"
#include "Versions/Version.h"
#include "rm2fb/SharedBuffer.h"

#include "swtcon.h"

namespace {
struct AddressInfo : public AddressInfoBase {
  static update_data mapUpdate(const UpdateParams& params) {
    update_data res = {
      .y0 = params.y1,
      .x0 = params.x1,
      .y1 = params.y2,
      .x1 = params.x2,
      .flags = params.flags,
      .update_mode = params.waveform,
      .zero = 0,
      .pixel_mode = params.extraMode,
    };

    if ((params.waveform & UpdateParams::ioctl_waveform_flag) == 0) {
      return res;
    }

    res.update_mode &= ~UpdateParams::ioctl_waveform_flag;
    res.update_mode = [&] {
      switch (res.update_mode) {
        case WAVEFORM_MODE_INIT:
          return 2;
        case WAVEFORM_MODE_DU:
        default:
          return 1;
        case WAVEFORM_MODE_GC16:
          return 2;
        case WAVEFORM_MODE_GL16:
          return 3;
        case WAVEFORM_MODE_A2:
          return 6;
      }
    }();

    // If the 'priority' bit is set.
    if ((params.flags & 4) != 0) {
      // Match the 'pen' modes in xochitl.
      res.flags = 2;
      // Don't use 7, as that'd use the backBuffer, which is not set.
      res.pixel_mode = 6;
    } else if ((params.flags & 0x1) == 0) {
      // Not full update, set the default 'extraMode' to 6.
      res.flags = 0;
      res.pixel_mode = 9;
    } else {
      // Full update
      res.flags = 1;
      res.pixel_mode = 9;
    }

    return res;
  }

  void initThreads() const final {
    const auto& fb = SharedFB::getInstance();
    // xochitl runs its own, separate swtcon instance (unmodified, via
    // ClientSwtcon.cpp) and always takes /tmp/epd.lock itself - skip it
    // here rather than fight over it, and rely on suspendForXochitl()/
    // resumeForXochitl() (SIGSTOP/SIGCONT-coordinated, see Server.cpp's
    // pause()/resume()) for mutual exclusion instead.
    if (swtcon_init(fb.getFb(), fb.getGrayBuffer(), /*skipPidLock=*/true) ==
        nullptr) {
      std::cerr << "swtcon_init failed\n";
      std::exit(EXIT_FAILURE);
    }
  }

  bool doUpdate(const UpdateParams& params) const final {
    update_data req = mapUpdate(params);

    swtcon_lock();
    swtcon_update(&req);
    swtcon_unlock_post();
    if (req.flags & Sync) {
      swtcon_wait();
    }

    return true;
  }

  void shutdownThreads() const final { swtcon_shutdown(0); }

  void suspendForXochitl() const final { swtcon_suspend(); }
  void resumeForXochitl() const final { swtcon_resume(); }

  bool installHooks(UpdateFn* newUpdate) const final {
    std::cerr << "Install hooks not supported on server\n";
    std::exit(EXIT_FAILURE);
  }
};
} // namespace

int
main(int argc, char* argv[], char** envp) {
  AddressInfo addrs;
  return serverMain(argv[0], &addrs);
}
