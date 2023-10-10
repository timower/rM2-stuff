#pragma once

#include <tuple>

namespace unistdpp {

/// Specialize this trait to specify how an argument or return value of the
/// given type should be translated to the C API.
template<typename T>
struct WrapperTraits {
  static T arg(T t) { return t; }
  static T ret(T t) { return t; }
};

namespace details {
template<typename T>
struct Expander {
  static std::tuple<T> expand(T t) { return t; }
};

template<typename... Ts>
struct Expander<std::tuple<Ts...>> {
  static std::tuple<Ts...> expand(std::tuple<Ts...> t) { return t; }
};
} // namespace details

template<auto, class>
struct FnWrapper;

/// Exposes lower level C APIs to C++, using the wrapper traits to translate
/// arguments.
template<auto Fn, typename Res, typename... Args>
struct FnWrapper<Fn, Res(Args...)> {

  template<typename... WrappedArgs>
  auto expand(WrappedArgs... args) const noexcept {
    return std::tuple_cat(details::Expander<WrappedArgs>::expand(args)...);
  }

  [[nodiscard]] auto operator()(Args... args) const noexcept {
    auto argsTuple = expand(WrapperTraits<Args>::arg(args)...);
    return WrapperTraits<Res>::ret(std::apply(Fn, std::move(argsTuple)));
  }
};
} // namespace unistdpp
