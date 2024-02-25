#pragma once

#include <functional>
#include <type_traits>

namespace detail {

template <class Func, class Ret, class... Args>
void has_signature(Ret (Func::* /*unused*/)(Args...)) {}

template <class Func, class Ret, class... Args>
void has_signature(Ret (Func::* /*unused*/)(Args...) const) {}

} // namespace detail

template <class>
class strongly_typed_function;

template <class Ret, class... Args>
class strongly_typed_function<Ret(Args...)> : public std::function<Ret(Args...)> {

public:
    // NOLINTNEXTLINE(google-explicit-constructor)
    strongly_typed_function(Ret (*f)(Args...)) : std::function<Ret(Args...)>(f) {}

    template <
        class Func,
        class = std::void_t<decltype(detail::has_signature<Func, Ret, Args...>(&Func::operator()))>>
    // NOLINTNEXTLINE(google-explicit-constructor)
    strongly_typed_function(Func f) : std::function<Ret(Args...)>(std::move(f)) {}

    strongly_typed_function(const strongly_typed_function&) = default;
    strongly_typed_function(strongly_typed_function&&) noexcept = default;
    strongly_typed_function& operator=(const strongly_typed_function&) = default;
    strongly_typed_function& operator=(strongly_typed_function&&) noexcept = default;
    ~strongly_typed_function() = default;
};
