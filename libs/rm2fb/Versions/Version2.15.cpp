#include "Version.h"

#include "AddressHooking.h"
#include "ImageHook.h"

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

  void initThreads() const final { createThreads.call<int, void*>(fb.mem); }

  bool doUpdate(const UpdateParams& params) const final {
    // 3.3 introduced the extra args. They shouldn't hurt on updates without
    // them.
    return update.call<bool, const UpdateParams*, int*, int>(
      &params, nullptr, 0);
  }
  void shutdownThreads() const final { shutdownFn.call<void>(); }

  bool installHooks(UpdateFn* newUpdate) const final {
    createThreads.hook((void*)createThreadsHook);
    update.hook((void*)newUpdate);
    shutdownFn.hook((void*)shutdownHook);
    waitForStart.hook((void*)waitHook);
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
