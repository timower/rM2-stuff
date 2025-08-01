#include "Version.h"

#include <cstring>
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <map>

namespace {

// The build ID in xochitl binaries happens to be at this address for all
// supported versions. See:
// readelf --sections /usr/bin/xochitl
//   [ 2] .note.gnu.bu[...] NOTE            00010170
// To get the value:
// xxd -s 384 -l 20 -i xochitl
constexpr auto xochitl_buildid_addr = 0x10180;

const AddressInfoBase*
getAddressesById(BuildId version) {
  static const std::map<BuildId, const AddressInfoBase*> addresses = {
    { { 0x93, 0x88, 0x6b, 0x15, 0xc3, 0xf5, 0x05, 0x12, 0x04, 0x9e,
        0xf2, 0x50, 0xc3, 0x20, 0xae, 0x00, 0x2a, 0x96, 0x5b, 0x05 },
      version_2_15_1 },

    { { 0x5c, 0x36, 0xb5, 0xbc, 0x41, 0xd9, 0xca, 0x8e, 0x77, 0x72,
        0x0d, 0xa6, 0x4d, 0x37, 0x91, 0x73, 0x70, 0x40, 0xfe, 0x1e },
      version_3_3_2 },

    { { 0x2e, 0xe6, 0x56, 0xd2, 0x5e, 0x17, 0xaf, 0x73, 0x96, 0x44,
        0x18, 0x0c, 0xf4, 0x7c, 0x0a, 0xd2, 0xf1, 0xd0, 0x3a, 0xe1 },
      version_3_5_2 },

    { { 0x31, 0xf4, 0xe9, 0xe8, 0x52, 0xaf, 0x39, 0x38, 0x15, 0xd8,
        0x14, 0xcd, 0xd3, 0xac, 0xb6, 0xdf, 0xbd, 0xc8, 0xb5, 0x67 },
      version_3_8_2 },

    // xochitl
    { { 0x4f, 0xa0, 0xe9, 0x51, 0x3d, 0x15, 0x53, 0xf3, 0x4d, 0xdc,
        0x89, 0xba, 0x65, 0xbd, 0xd5, 0x69, 0x92, 0x92, 0x93, 0x9a },
      version_3_20_0 },
    // libqsgepaper
    { { 0x0f, 0x18, 0xb6, 0x39, 0xf6, 0x05, 0x2d, 0xdb, 0xf5, 0x89,
        0x35, 0x62, 0x45, 0xdb, 0x66, 0xc9, 0x96, 0x96, 0xbe, 0xe7 },
      version_3_20_0 },
  };

  auto it = addresses.find(version);
  if (it == addresses.end()) {
    return nullptr;
  }

  return it->second;
}

BuildId
getXochitlBuildId() {
  BuildId id;

  memcpy(
    id.data(), reinterpret_cast<const char*>(xochitl_buildid_addr), id.size());

  return id;
}

} // namespace

const AddressInfoBase*
getAddresses(std::optional<BuildId> id) {
  if (!id.has_value()) {
    id = getXochitlBuildId();
  }

  std::cerr << "Build ID: ";
  for (unsigned char c : *id) {
    std::cerr << "0x" << std::hex << std::setfill('0') << std::setw(2) << int(c)
              << " ";
  }
  std::cerr << "\n";

  return getAddressesById(*id);
}

void*
getQsgepaperHandle() {
  static auto* handle = []() -> void* {
    auto* handle =
      dlopen("/usr/lib/plugins/scenegraph/libqsgepaper.so", RTLD_LAZY);
    if (handle == nullptr) {
      std::cerr << dlerror() << "\n";
      return nullptr;
    }
    return handle;
  }();

  return handle;
}
