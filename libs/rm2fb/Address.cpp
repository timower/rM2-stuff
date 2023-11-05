#include "Address.h"
#include "AddressHooking.h"

#include <fstream>
#include <map>
#include <variant>

namespace {

using FunctionInfo = std::variant<SimpleFunction, InlinedFunction>;

void
new_create_threads() {
  puts("HOOK: create threads called");
}

int
new_shutdown() {
  puts("HOOK: shutdown called");
  return 0;
}

struct AddressInfo : public AddressInfoBase {

  /// Function that creates the QObject containing the EDP framebuffer.
  /// This will create the threads that update the frames, and wait for them to
  /// be initialized. Used since xochitl 3.5+ as the function that creates
  /// threads was inlined into it. We replace the framebuffer storage by symbol
  /// hooking the QImage constructor.
  ///
  /// Find it in Ghidra by looking for the string 'Unable to start vsync and
  /// flip thread' and going to the caller.
  FunctionInfo createInstance;

  /// Sends an update message to the update threads, refreshing the screen.
  ///
  /// Also one of the few functions that calls usleep
  /// (to implement synchronous updates).
  FunctionInfo update;

  /// stops the update threads cleanly.
  ///
  /// Can be found by looking for the string "Shutting down.. "
  FunctionInfo shutdownThreadsFunction;

  AddressInfo(FunctionInfo createInstance,
              FunctionInfo update,
              FunctionInfo shutdownThreads)
    : createInstance(std::move(createInstance))
    , update(std::move(update))
    , shutdownThreadsFunction(std::move(shutdownThreads)) {}

  void initThreads() const final {
    std::visit(
      [](auto createInstance) { createInstance.template call<void*>(); },
      createInstance);
  }

  bool doUpdate(const UpdateParams& params) const final {
    return std::visit(
      [&params](auto update) {
        return update.template call<bool, const UpdateParams*>(&params);
      },
      update);
  }

  void shutdownThreads() const final {
    std::visit([](auto shutdown) { shutdown.template call<void>(); },
               shutdownThreadsFunction);
  }

  bool installHooks(UpdateFn* newUpdate) const final {
    bool result = true;

    // Hook create Instance
    result = std::visit(
      [](auto createInstance) {
        return createInstance.hook((void*)new_create_threads);
      },
      createInstance);

    if (!result)
      return false;

    // Hook update
    result = std::visit(
      [&](auto update) { return update.hook((void*)newUpdate); }, update);

    if (!result)
      return false;

    // Hook shutdown
    result = std::visit(
      [](auto shutdown) { return shutdown.hook((void*)new_shutdown); },
      shutdownThreadsFunction);

    return result;
  }
};

const std::map<std::string_view, AddressInfo> addresses = {

  // 2.15
  { "20221026104022",
    AddressInfo{ SimpleFunction{ 0x4e7520 },
                 SimpleFunction{ 0x4e65dc },
                 SimpleFunction{ 0x4e74b8 },
                 /*.waitForThreads = 0x4e64c0*/ } },

  // 3.3
  { "20230414143852",
    { SimpleFunction{ 0x55b504 },
      SimpleFunction{ 0x55a4c8 },
      SimpleFunction{ 0x55b494 },
      /*.waitForThreads = 0x55a39c*/ } },

  // 3.5
  { "20230608125139",
    { InlinedFunction{ 0x59fd0, 0x5a0e8, 0x5b4c0 },
      SimpleFunction{ 0x53ff8 },
      SimpleFunction{ 0xb2f08 },
      /*.waitForThreads = 0x47df8c*/ } },
};

const AddressInfoBase*
getAddresses(std::string_view version) {
  auto it = addresses.find(version);
  if (it == addresses.end()) {
    return nullptr;
  }
  const auto& addressInfo = it->second;

  return &addressInfo;
}

} // namespace

const AddressInfoBase*
getAddresses(const char* path) {
  const auto version_file = path == nullptr ? "/etc/version" : path;
  std::ifstream ifs(version_file);
  if (!ifs.is_open()) {
    return nullptr;
  }

  std::string version;
  ifs >> version;

  return getAddresses(version);
}
