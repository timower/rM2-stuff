#include "Server.h"
#include "Versions/Version.h"
#include "rm2fb/SharedBuffer.h"

#include "swtcon.h"

#include <rm2.h>

namespace {

// rm2.h documents this equivalence; mapUpdate's flag remap below is just a
// mask because of it.
static_assert(RM2_FLAG_SYNC == UpdateFlags::Sync);
static_assert(RM2_FLAG_FAST_DRAW == UpdateFlags::FastDraw);

// Translates a WAVEFORM_MODE_* ioctl constant into swtcon's own waveform
// LUT index.
SwtconWaveformMode
mapWaveformMode(int waveform) {
  switch (waveform & ~UpdateParams::ioctl_waveform_flag) {
    case WAVEFORM_MODE_INIT:
      return SwtconWaveformMode::GC16;
    case WAVEFORM_MODE_DU:
      return SwtconWaveformMode::DU;
    case WAVEFORM_MODE_GC16:
      return SwtconWaveformMode::GC16;
    case WAVEFORM_MODE_GL16:
    default:
      return SwtconWaveformMode::GL16;
    case WAVEFORM_MODE_A2:
      return SwtconWaveformMode::A2;
  }
}

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

    res.update_mode = static_cast<int>(mapWaveformMode(params.waveform));

    // RM2UpdateFlags matches swtcon's own UpdateFlags bit-for-bit, so the
    // only remapping needed is masking down to the bits swtcon knows.
    res.flags = params.flags & (RM2_FLAG_SYNC | RM2_FLAG_FAST_DRAW);

    // Auto-select the pixel formula from the waveform mode (see
    // render_kernel_case's dispatch table) instead of hand-picking one -
    // never resolves to case 7, so this stays safe with no backBuffer set.
    res.pixel_mode = 5;

    return res;
  }

  void initThreads() const final {
    const auto& fb = getGlobalFrameBuffer();
    // xochitl runs its own, separate swtcon instance (unmodified, via
    // ClientSwtcon.cpp) and always takes /tmp/epd.lock itself - skip it
    // here rather than fight over it, and rely on suspendForXochitl()/
    // resumeForXochitl() (SIGSTOP/SIGCONT-coordinated, see Server.cpp's
    // pause()/resume()) for mutual exclusion instead.
    if (swtcon_init({ .dataBuffer = fb.getFb(),
                      .backBuffer = fb.getGrayBuffer(),
                      .skipPidLock = true }) == nullptr) {
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
