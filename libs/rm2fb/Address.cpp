#include "Address.h"

#include <fstream>
#include <map>

namespace {
const std::map<std::string_view, AddressInfo> addresses = {

  // 2.15
  { "20221026104022",
    { .createInstance = SimpleFunction{ 0x4e7520 },
      .update = SimpleFunction{ 0x4e65dc },
      .shutdownThreads = SimpleFunction{ 0x4e74b8 },
      /*.waitForThreads = 0x4e64c0*/ } },

  // 3.3
  { "20230414143852",
    { .createInstance = SimpleFunction{ 0x55b504 },
      .update = SimpleFunction{ 0x55a4c8 },
      .shutdownThreads = SimpleFunction{ 0x55b494 },
      /*.waitForThreads = 0x55a39c*/ } },

  // 3.5
  { "20230608125139",
    { .createInstance = InlinedFunction{ 0x59fd0, 0x5a0e8, 0x5b4c0 },
      .update = SimpleFunction{ 0x53ff8 },
      .shutdownThreads = SimpleFunction{ 0xb2f08 },
      /*.waitForThreads = 0x47df8c*/ } },
};

std::optional<AddressInfo>
getAddresses(std::string_view version) {
  auto it = addresses.find(version);
  if (it == addresses.end()) {
    return {};
  }
  const auto& addressInfo = it->second;

  return addressInfo;
}
} // namespace

std::optional<AddressInfo>
getAddresses(const char* path) {
  const auto version_file = path == nullptr ? "/etc/version" : path;
  std::ifstream ifs(version_file);
  if (!ifs.is_open()) {
    return {};
  }

  std::string version;
  ifs >> version;

  return getAddresses(version);
}
