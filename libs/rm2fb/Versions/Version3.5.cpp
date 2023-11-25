#include "AddressHooking.h"
#include "Version.h"

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

struct AddressInfo : public AddressInfoBase {
  InlinedFunction createThreads{};
  SimpleFunction update{};
  SimpleFunction shutdownFn{};

  AddressInfo(InlinedFunction createThreads,
              SimpleFunction update,
              SimpleFunction shutdownFn)
    : createThreads(createThreads), update(update), shutdownFn(shutdownFn) {}

  void initThreads() const final { createThreads.call<void*>(); }

  bool doUpdate(const UpdateParams& params) const final {
    UpdateParams mappedParams = params;
    mappedParams.waveform = mapWaveform(params.waveform);
    return update.call<bool, const UpdateParams*>(&mappedParams);
  }
  void shutdownThreads() const final { shutdownFn.call<void>(); }

  bool installHooks(UpdateFn* newUpdate) const final {
    createThreads.hook((void*)createThreadsHook);
    update.hook((void*)newUpdate);
    shutdownFn.hook((void*)shutdownHook);
    return true;
  }
};

const AddressInfo version_3_5_info = AddressInfo{
  InlinedFunction{ 0x59fd0, 0x5a0e8, 0x5b4c0 },
  SimpleFunction{ 0x53ff8 },
  SimpleFunction{ 0xb2f08 },
};
} // namespace

const AddressInfoBase* const version_3_5_2 = &version_3_5_info;
