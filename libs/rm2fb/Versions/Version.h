#pragma once

#include "rm2fb/Message.h"
#include <rm2.h>

#include <array>
#include <optional>
#include <vector>

using BuildId = std::array<unsigned char, 20>;

class AddressInfoBase {
public:
  using UpdateFn = bool(const UpdateParams&);

  // Server API:
  virtual void initThreads() const = 0;
  virtual bool doUpdate(const UpdateParams& params) const = 0;

  // Default applies each update individually; ServerSwtcon.cpp overrides
  // this for real batching via swtcon_lock()/swtcon_unlock_post().
  virtual bool doUpdateBatch(const std::vector<UpdateParams>& updates) const {
    bool ok = true;
    for (auto& p : updates) {
      ok = doUpdate(p) && ok;
    }
    return ok;
  }

  virtual void shutdownThreads() const = 0;

  // Hooks for coexisting with a client that runs its own, independent
  // swtcon instance (currently only xochitl, via ClientSwtcon.cpp/
  // rm2fb_client_swtcon) rather than being driven through doUpdate().
  // Called by Server.cpp around pausing/resuming that client, so this
  // server's own swtcon instance (if any) stays out of the panel's way
  // while the coexisting client owns it, and drives it again once that
  // client is paused. No-op by default: only the swtcon-backed server
  // (ServerSwtcon.cpp) owns a swtcon instance of its own to suspend/
  // resume here - the by-address hooking implementations don't have a
  // separate swtcon instance, since they drive xochitl's own qsgepaper
  // directly via doUpdate() instead.
  virtual void suspendForXochitl() const {}
  virtual void resumeForXochitl() const {}

  // Client API:
  virtual bool installHooks(UpdateFn* newUpdate) const = 0;

  // mappings of different xochitls:
  // mxcfb | 2.15 | 3.3 | 3.5
  // ------+------+-----+----
  // 0 INIT| 2    | 2   | 2
  // 1 DU  | 0    | 0   | 0
  // 2 GC16| 1    | 3   | 3
  // 3 GL16| 2    | 2   | 2
  // 4 A2  | /    | 0/8 | 0/8
  // 5 GC? | /    | 1   | 1
  // 6 Pan | 3    | 0   | 0
  // 7 ?   | /    | 0   | 0
  // 8 ?   | 1    | 0   | 0
  static int mapWaveform(int waveform) {
    if ((waveform & UpdateParams::ioctl_waveform_flag) == 0) {
      return waveform;
    }

    waveform &= ~UpdateParams::ioctl_waveform_flag;
    switch (waveform) {
      case WAVEFORM_MODE_INIT:
        return 1;
      case WAVEFORM_MODE_DU:
      default:
        return 0;
      case WAVEFORM_MODE_GC16:
        return 1;
      case WAVEFORM_MODE_GL16:
        return 2;
      case WAVEFORM_MODE_A2:
        return 3;
    }
  }

  virtual ~AddressInfoBase() = default;
};

extern const AddressInfoBase* const version_2_15_1;
extern const AddressInfoBase* const version_3_3_2;
extern const AddressInfoBase* const version_3_5_2;
extern const AddressInfoBase* const version_3_8_2;
extern const AddressInfoBase* const version_3_20_0;
extern const AddressInfoBase* const version_3_22_0;
extern const AddressInfoBase* const version_3_22_4;
extern const AddressInfoBase* const version_3_23_0_54;
extern const AddressInfoBase* const version_3_23_0_64;

const AddressInfoBase*
getAddresses(std::optional<BuildId> id = std::nullopt);

void*
getQsgepaperHandle();
