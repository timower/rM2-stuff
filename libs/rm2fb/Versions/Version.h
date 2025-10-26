#pragma once

#include "Message.h"
#include <rm2.h>

#include <array>
#include <optional>

using BuildId = std::array<unsigned char, 20>;

class AddressInfoBase {
public:
  using UpdateFn = bool(const UpdateParams&);

  // Server API:
  virtual void initThreads() const = 0;
  virtual bool doUpdate(const UpdateParams& params) const = 0;
  virtual void shutdownThreads() const = 0;

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

const AddressInfoBase*
getAddresses(std::optional<BuildId> id = std::nullopt);

void*
getQsgepaperHandle();
