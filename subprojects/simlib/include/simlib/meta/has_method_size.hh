#pragma once

#include <type_traits>
#include <utility>

namespace meta {

namespace detail {

template <class T, class = decltype(std::declval<T>().size())>
constexpr auto has_method_size(int) -> std::true_type;
template <class>
constexpr auto has_method_size(...) -> std::false_type;

} // namespace detail

template <class T>
constexpr inline bool has_method_size = decltype(detail::has_method_size<T>(0))::value;

} // namespace meta
