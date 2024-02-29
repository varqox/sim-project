#pragma once

#include <ostream>
#include <type_traits>
#include <utility>

namespace detail {

template <class T, class = decltype(std::declval<std::ostream&>() << std::declval<T>())>
constexpr auto is_printable(int) -> std::true_type;
template <class>
constexpr auto is_printable(...) -> std::false_type;

} // namespace detail

template <class T>
constexpr inline bool is_printable = decltype(detail::is_printable<T>(0))::value;
