#pragma once

#include <cassert>
#include <cstddef>
#include <tuple>

template<typename T>
using Ptr = T*;

// X macro to define functions we can hook using preload.
//
// X(ID, name, return type, args...)
#define HOOKS(X)                                                               \
  X(Malloc, Ptr<void>, malloc, size_t(size))                                   \
  X(Calloc, Ptr<void>, calloc, size_t(size), size_t(count))

class PreloadHook {
  using HookTuple = std::tuple<
#define HOOK_TY(id, res, name, ...) res (*)(res(*)(__VA_ARGS__), __VA_ARGS__),
    HOOKS(HOOK_TY) std::nullptr_t>;

public:
  enum HookId {
#define ENUMX(name, ...) name,
    HOOKS(ENUMX)
  };

  template<HookId id>
  bool isHooked() const {
    return std::get<id>(hookPtrs) != nullptr;
  }

  template<HookId id>
  auto* getHook() const {
    return std::get<id>(hookPtrs);
  }

  template<HookId id>
  void unhook() {
    std::get<id>(hookPtrs) = nullptr;
  }

  template<HookId id>
  void hook(std::tuple_element_t<id, HookTuple> func) {
    assert(!isHooked<id>() && "Shouldn't hook twice!");
    std::get<id>(hookPtrs) = func;
  }

  static PreloadHook& getInstance();

  PreloadHook(const PreloadHook&) = delete;
  PreloadHook(PreloadHook&&) = delete;
  PreloadHook& operator=(const PreloadHook&) = delete;
  PreloadHook& operator=(PreloadHook&&) = delete;

private:
  PreloadHook() = default;

  HookTuple hookPtrs;
};
