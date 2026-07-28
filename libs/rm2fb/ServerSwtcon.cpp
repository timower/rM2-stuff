#include "Server.h"
#include "Versions/Version.h"
#include "rm2fb/SharedBuffer.h"

#include "swtcon.h"

namespace {
struct AddressInfo : public AddressInfoBase {

  void initThreads() const final {
    const auto& fb = SharedFB::getInstance();
    swtcon_init(fb.getFb(), fb.getGrayBuffer());
  }

  bool doUpdate(const UpdateParams& params) const final {
    update_data req = {
      .y0 = params.y1,
      .x0 = params.x1,
      .y1 = params.y2,
      .x1 = params.x2,
      .flags = params.flags,
      .update_mode = params.waveform,
      .zero = 0,
      .pixel_mode = params.extraMode,
    };

    swtcon_lock();
    swtcon_update(&req);
    swtcon_unlock_post();
    if (params.flags & Sync) {
      swtcon_wait();
    }

    return true;
  }

  void shutdownThreads() const final { swtcon_shutdown(0); }

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
