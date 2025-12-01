#include "Version.h"

#include "AddressHooking.h"
#include "ImageHook.h"
#include "PreloadHooks.h"
#include "rm2fb/SharedBuffer.h"

#include <cstring>
#include <rm2.h>

namespace {

void
createThreadsHook() {
  puts("HOOK: create threads called");
}

int
shutdownHook() {
  puts("HOOK: shutdown called");
  return 0;
}

int
waitHook() {
  puts("HOOK: wait called");
  return 0;
}

struct AddressInfo : public AddressInfoBase {
  SimpleFunction createThreads{};
  SimpleFunction update{};
  SimpleFunction shutdownFn{};

  SimpleFunction waitForStart{};

  AddressInfo(SimpleFunction createThreads,
              SimpleFunction update,
              SimpleFunction shutdownFn,
              SimpleFunction waitForStart)
    : createThreads(createThreads)
    , update(update)
    , shutdownFn(shutdownFn)
    , waitForStart(waitForStart) {}

  void initThreads() const final {
    const auto& fb = SharedFB::getInstance();
    if (!fb.isValid()) {
      return;
    }

    // createThreads can init the framebuffer data, but if rm2fb server was
    // started by a systemd socket, the client could have already written to it.
    // So we save a copy and restore it. The updates will be buffered in the
    // control socket.
    // auto* fbMem = static_cast<std::uint8_t*>(fb->getFb());
    // std::vector<std::uint8_t> fbCopy(fbMem, fbMem + fb_size);

    createThreads.call<int, void*>(fb.getFb());
    waitForStart.call<void>();

    // std::memcpy(fb->mem.get(), fbCopy.data(), fbCopy.size());
  }

  bool doUpdate(const UpdateParams& params) const final {
    UpdateParams mappedParams = params;
    mappedParams.waveform = mapWaveform(params.waveform);

    // 3.3 introduced the extra args. They shouldn't hurt on updates without
    // them.
    return update.call<bool, const UpdateParams*, int*, int>(
      &mappedParams, nullptr, 0);
  }
  void shutdownThreads() const final { shutdownFn.call<void>(); }

  bool installHooks(UpdateFn* newUpdate) const final {
    createThreads.hook((void*)createThreadsHook);
    update.hook((void*)newUpdate);
    shutdownFn.hook((void*)shutdownHook);
    waitForStart.hook((void*)waitHook);
    PreloadHook::getInstance().hook<PreloadHook::QImageCtor>(qimageHook);
    return true;
  }
};

const AddressInfo version_2_15_info = AddressInfo{
  SimpleFunction{ 0x4e7520 },
  SimpleFunction{ 0x4e65dc },
  SimpleFunction{ 0x4e74b8 },
  SimpleFunction{ 0x4e64c0 },
};

const AddressInfo version_3_3_info = AddressInfo{
  SimpleFunction{ 0x55b504 },
  SimpleFunction{ 0x55a4c8 },
  SimpleFunction{ 0x55b494 },
  SimpleFunction{ 0x55a39c },
};

} // namespace

const AddressInfoBase* const version_2_15_1 = &version_2_15_info;
const AddressInfoBase* const version_3_3_2 = &version_3_3_info;
