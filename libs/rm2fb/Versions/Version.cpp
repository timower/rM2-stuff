#include "Version.h"

#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>

namespace {
// xxd -s 384 -l 20 -i xochitl
using BuildId = std::array<unsigned char, 20>;

const AddressInfoBase*
getAddresses(BuildId version) {
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
  };

  auto it = addresses.find(version);
  if (it == addresses.end()) {
    return nullptr;
  }

  return it->second;
}

} // namespace

const AddressInfoBase*
getAddresses() {
  BuildId id;

  // NOLINTNEXTLINE
  memcpy(id.data(), reinterpret_cast<const char*>(0x10180), id.size());

  std::cerr << "Build ID: ";
  for (unsigned char c : id) {
    std::cerr << "0x" << std::hex << std::setfill('0') << std::setw(2) << int(c)
              << " ";
  }
  std::cerr << "\n";
  return getAddresses(id);
}
