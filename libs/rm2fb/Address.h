#pragma once

#include "Message.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

class AddressInfoBase {
public:
  using UpdateFn = bool(const UpdateParams&);
  // Server API:
  virtual void initThreads() const = 0;
  virtual bool doUpdate(const UpdateParams& params) const = 0;
  virtual void shutdownThreads() const = 0;

  // Client API:
  virtual bool installHooks(UpdateFn* newUpdate) const = 0;
};

const AddressInfoBase*
getAddresses(const char* path = nullptr);
