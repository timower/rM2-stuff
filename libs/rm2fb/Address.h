#pragma once

#include "Message.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

template<typename Res, typename... Args>
Res
callPtr(uintptr_t ptr, Args... args) {
  auto* fnPtr = reinterpret_cast<Res (*)(Args...)>(ptr);
  return fnPtr(args...);
}

struct SimpleFunction {
  uintptr_t address;

  template<typename Res, typename... Args>
  Res call(Args... args) {
    return callPtr<Res>(address, args...);
  }

  // Replaces the function with the given replacement, arguemnts are forwarded
  // and the return address is replaced.
  bool hook(void* replacement);
};

struct InlinedFunction {
  uintptr_t callAddress;

  uintptr_t hookStart;
  uintptr_t hookEnd;

  template<typename Res, typename... Args>
  Res call(Args... args) {
    return callPtr<Res>(callAddress, args...);
  }

  // Hooks the inlined function, but doesn't forward arguments and doesn't use
  // the return value!
  bool hook(void* replacement);
};

using FunctionInfo = std::variant<SimpleFunction, InlinedFunction>;

struct AddressInfo {

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
  FunctionInfo shutdownThreads;
};

std::optional<AddressInfo>
getAddresses(const char* path = nullptr);
