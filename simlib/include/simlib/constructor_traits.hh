#pragma once

#include <type_traits>
#include <utility>

namespace detail {

template <class T, class... Args>
auto test_implicitly_constructible(int) -> decltype(
        void(std::declval<void (&)(T)>()({std::declval<Args>()...})), std::true_type{});
template <class T, class... Args>
auto test_implicitly_constructible(...) -> std::false_type;

} // namespace detail

template <class T, class... Args>
constexpr inline bool is_implicitly_constructible_v = std::is_constructible_v<T,
        Args...>and decltype(detail::test_implicitly_constructible<T, Args...>(0))::value;

template <class ConstructorClass, class... ConstructorArgs>
constexpr inline bool is_constructor_ok = !((sizeof...(ConstructorArgs) == 1) and ... and
        std::is_same_v<ConstructorClass, std::decay_t<ConstructorArgs>>);

template <class ConstructorClass, class BaseClass, class... ConstructorArgs>
constexpr inline bool is_inheriting_explicit_constructor_ok =
        is_constructor_ok<ConstructorClass, ConstructorArgs...>and
                std::is_constructible_v<BaseClass, ConstructorArgs...> and
        !is_implicitly_constructible_v<BaseClass, ConstructorArgs...>;

template <class ConstructorClass, class BaseClass, class... ConstructorArgs>
constexpr inline bool is_inheriting_implicit_constructor_ok =
        is_constructor_ok<ConstructorClass, ConstructorArgs...>and
                is_implicitly_constructible_v<BaseClass, ConstructorArgs...>;
