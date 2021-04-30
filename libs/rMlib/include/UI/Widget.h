#pragma once

#include <cassert>
#include <memory>

namespace rmlib {

class RenderObject;

namespace details {
template<typename Fn>
struct ArgType;

template<typename R, typename Class, typename Arg>
struct ArgType<R (Class::*)(Arg)> {
  using Type = Arg;
};
} // namespace details

template<typename RO>
class Widget {

public:
  std::unique_ptr<RenderObject> createRenderObject() const {
    assert(false && "must implement");
    return nullptr;
  }

  void update(RenderObject& ro) const {
    static_assert(std::is_base_of_v<RenderObject, RO>,
                  "RO must derive from RenderObject");
    using ArgType = typename details::ArgType<decltype(&RO::update)>::Type;
    static_cast<RO*>(&ro)->update(static_cast<ArgType>(*this));
  }
};

} // namespace rmlib
