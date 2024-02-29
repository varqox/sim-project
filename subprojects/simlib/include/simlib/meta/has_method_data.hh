#pragma once

#include <type_traits>
#include <utility>

namespace meta {

namespace detail {

template <class T, class = decltype(std::declval<T>().data())>
constexpr auto has_method_data(int) -> std::true_type;
template <class>
constexpr auto has_method_data(...) -> std::false_type;

} // namespace detail

template <class T>
constexpr inline bool has_method_data = decltype(detail::has_method_data<T>(0))::value;

} // namespace meta
