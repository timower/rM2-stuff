#pragma once

#include <cassert>
#include <functional>
#include <memory>

namespace rmlib {

/// Who knew futures were complicated?

template<typename T>
class Promise;

template<typename T>
class Future;

namespace details {

/// This essentialy models a move-only std::function.
template<typename T>
class PromiseContinuation {
public:
  virtual void set(T) = 0;
  virtual ~PromiseContinuation() = default;
};

template<>
class PromiseContinuation<void> {
public:
  virtual void set() = 0;
  virtual ~PromiseContinuation() = default;
};

template<typename T, typename Func>
class FuncContinuation : public PromiseContinuation<T> {
public:
  FuncContinuation(Func&& func) : func(std::forward<Func>(func)) {}

  void set(T val) override { func(std::move(val)); }

private:
  Func func;
};

template<typename Func>
class FuncContinuation<void, Func> : public PromiseContinuation<void> {
public:
  FuncContinuation(Func&& func) : func(std::forward<Func>(func)) {}

  void set() override { func(); }

private:
  Func func;
};

/// The shared state owned by Promise and referenced by Future. On
/// `Promise.setValue` either `value` will get set or `continuation` will get
/// called.
template<typename Input>
class State {
public:
  std::optional<Input> value;
  std::unique_ptr<PromiseContinuation<Input>> continuation;
};

template<>
class State<void> {
public:
  bool value = false;
  std::unique_ptr<PromiseContinuation<void>> continuation = nullptr;
};

template<typename T>
class FutureTraits : public std::false_type {};

template<typename R>
class FutureTraits<Future<R>> : public std::true_type {
public:
  using ResultType = R;
};
} // namespace details

template<typename T>
class Future {
public:
  Future() = default;
  Future(const Future&) = delete;
  Future& operator=(const Future&) = delete;

  Future(Future&& other) : state(other.state) { other.state = nullptr; }
  Future& operator=(Future&& other) {
    state = other.state;
    other.state = nullptr;
    return *this;
  }

  /// \returns True if the future points to some shared state.
  bool valid() const { return this->state != nullptr; }

  /// \returns True if the shared state contains a value.
  bool hasValue() const { return this->state->value.has_value(); }

  /// \returns The value in the shared state.
  T getValue() {
    auto ret = this->state->value;
    this->state = nullptr;
    return std::move(ret);
  }

  /// Registers \p func to be called whenever the shared state gets its value.
  /// The function will be called with the value that is set. It can either
  /// return another value, or another future. If it returns another value the
  /// result is a future of that value. If it returns another future then the
  /// result is a future for the result of that future.
  ///
  /// Has no effect if the shared state already has value.
  template<typename Func>
  auto then(Func&& func) {
    using Result = std::result_of_t<Func(T)>;

    if constexpr (details::FutureTraits<Result>::value) {
      using FutResult = typename details::FutureTraits<Result>::ResultType;
      auto newPromise = Promise<FutResult>();
      auto newFuture = newPromise.getFuture();

      auto newFunc = [func = std::move(func),
                      prom = std::move(newPromise)](T val) mutable {
        auto newFut = func(std::move(val));
        if constexpr (std::is_void_v<FutResult>) {
          newFut.then([prom = std::move(prom)]() mutable { prom.setValue(); });
        } else {
          newFut.then([prom = std::move(prom)](FutResult res) mutable {
            prom.setValue(res);
          });
        }
      };
      auto cont =
        std::make_unique<details::FuncContinuation<T, decltype(newFunc)>>(
          std::move(newFunc));
      state->continuation = std::move(cont);

      return newFuture;

    } else {
      auto newPromise = Promise<Result>();
      auto newFuture = newPromise.getFuture();

      auto newFunc = [func = std::move(func),
                      prom = std::move(newPromise)](T val) mutable {
        if constexpr (std::is_void_v<Result>) {
          func(std::move(val));
          prom.setValue();
        } else {
          prom.setValue(func(std::move(val)));
        }
      };

      auto cont =
        std::make_unique<details::FuncContinuation<T, decltype(newFunc)>>(
          std::move(newFunc));
      state->continuation = std::move(cont);

      return newFuture;
    }
  }

private:
  friend class Promise<T>;

  Future(details::State<T>& state) : state(&state) {}

  details::State<T>* state = nullptr;
};

template<>
class Future<void> {
public:
  Future() = default;

  Future(const Future&) = delete;
  Future& operator=(const Future&) = delete;

  Future(Future&& other) : state(other.state) { other.state = nullptr; }
  Future& operator=(Future&& other) {
    state = other.state;
    other.state = nullptr;
    return *this;
  }

  bool valid() const { return state != nullptr; }

  bool hasValue() const { return state->value; }

  template<typename Func>
  auto then(Func&& func) {
    using Result = std::result_of_t<Func()>;
    if constexpr (details::FutureTraits<Result>::value) {
      using FutResult = typename details::FutureTraits<Result>::ResultType;
      auto newPromise = Promise<FutResult>();
      auto newFuture = newPromise.getFuture();

      auto newFunc = [func = std::move(func),
                      prom = std::move(newPromise)]() mutable {
        auto newFut = func();
        if constexpr (std::is_void_v<FutResult>) {
          newFut.then([prom = std::move(prom)]() mutable { prom.setValue(); });
        } else {
          newFut.then([prom = std::move(prom)](FutResult res) mutable {
            prom.setValue(res);
          });
        }
      };
      auto cont =
        std::make_unique<details::FuncContinuation<void, decltype(newFunc)>>(
          std::move(newFunc));
      state->continuation = std::move(cont);

      return newFuture;

    } else {
      auto newPromise = Promise<Result>();
      auto newFuture = newPromise.getFuture();

      auto newFunc = [func = std::move(func),
                      prom = std::move(newPromise)]() mutable {
        if constexpr (std::is_void_v<Result>) {
          func();
          prom.setValue();
        } else {
          prom.setValue(func());
        }
      };

      auto cont =
        std::make_unique<details::FuncContinuation<void, decltype(newFunc)>>(
          std::move(newFunc));
      state->continuation = std::move(cont);

      return newFuture;
    }
  }

private:
  template<typename U>
  friend class Promise;

  Future(details::State<void>& state) : state(&state) {}

  details::State<void>* state = nullptr;
};

template<typename T>
class Promise {
public:
  Promise() {
    auto initState = std::make_unique<details::State<T>>();
    future = Future<T>(*initState);
    ptr = std::move(initState);
  }

  Promise(Promise&& other) = default;
  Promise& operator=(Promise&& other) = default;

  Promise(const Promise& other) = delete;
  Promise& operator=(const Promise& other) = delete;

  /// Sets the value in the shared state.
  void setValue(T value) {
    if (ptr->continuation) {
      ptr->continuation->set(std::move(value));
      return;
    }

    ptr->value = std::move(value);
  }

  /// Moves the future out of this promise. Can only be called once!
  Future<T> getFuture() { return std::move(future); }

private:
  friend class Future<T>;

  std::unique_ptr<details::State<T>> ptr;
  Future<T> future;
};

template<>
class Promise<void> {
public:
  Promise() {
    auto initState = std::make_unique<details::State<void>>();
    future = Future<void>(*initState);
    ptr = std::move(initState);
  }

  Promise(Promise&& other) = default;
  Promise& operator=(Promise&& other) = default;

  Promise(const Promise& other) = delete;
  Promise& operator=(const Promise& other) = delete;

  void setValue() {
    if (ptr->continuation) {
      ptr->continuation->set();
      return;
    }

    ptr->value = true;
  }

  Future<void> getFuture() { return std::move(future); }

private:
  friend class Future<void>;

  std::unique_ptr<details::State<void>> ptr;
  Future<void> future;
};

} // namespace rmlib
