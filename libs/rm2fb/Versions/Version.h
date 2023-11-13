#pragma once

#include "Message.h"

class AddressInfoBase {
public:
  using UpdateFn = bool(const UpdateParams&);

  // Server API:
  virtual void initThreads() const = 0;
  virtual bool doUpdate(const UpdateParams& params) const = 0;
  virtual void shutdownThreads() const = 0;

  // Client API:
  virtual bool installHooks(UpdateFn* newUpdate) const = 0;

  virtual ~AddressInfoBase() = default;
};

extern const AddressInfoBase* const version_2_15_1;
extern const AddressInfoBase* const version_3_3_2;
extern const AddressInfoBase* const version_3_5_2;

const AddressInfoBase*
getAddresses(const char* path = nullptr);
