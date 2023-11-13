#include "Version.h"

#include <fstream>
#include <map>

namespace {

const AddressInfoBase*
getAddresses(std::string_view version) {
  static const std::map<std::string_view, const AddressInfoBase*> addresses = {
    { "20221026104022", version_2_15_1 },
    { "20230414143852", version_3_3_2 },
    { "20230608125139", version_3_5_2 },
  };

  auto it = addresses.find(version);
  if (it == addresses.end()) {
    return nullptr;
  }

  return it->second;
}

} // namespace

const AddressInfoBase*
getAddresses(const char* path) {
  const auto* versionFile = path == nullptr ? "/etc/version" : path;
  std::ifstream ifs(versionFile);
  if (!ifs.is_open()) {
    return nullptr;
  }

  std::string version;
  ifs >> version;

  return getAddresses(version);
}
