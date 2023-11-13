#pragma once

#include <cstdint>

template<typename Res, typename... Args>
Res
callPtr(uintptr_t ptr, Args... args) {
  auto* fnPtr = reinterpret_cast<Res (*)(Args...)>(ptr);
  return fnPtr(args...);
}

struct SimpleFunction {
  uintptr_t address;

  template<typename Res, typename... Args>
  Res call(Args... args) const {
    return callPtr<Res>(address, args...);
  }

  // Replaces the function with the given replacement, arguemnts are forwarded
  // and the return address is replaced.
  bool hook(void* replacement) const;
};

struct InlinedFunction {
  uintptr_t callAddress;

  uintptr_t hookStart;
  uintptr_t hookEnd;

  template<typename Res, typename... Args>
  Res call(Args... args) const {
    return callPtr<Res>(callAddress, args...);
  }

  // Hooks the inlined function, but doesn't forward arguments and doesn't use
  // the return value!
  bool hook(void* replacement) const;
};
