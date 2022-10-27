#pragma once

#include <simlib/meta.hh>
#include <simlib/to_string.hh>
#include <type_traits>

template <class T, std::enable_if_t<meta::has_method_size<const T&>, int> = 0>
constexpr auto string_length(const T& str) noexcept {
    return str.size();
}

constexpr auto string_length(const char* str) noexcept {
    return std::char_traits<char>::length(str);
}

constexpr auto string_length(char* str) noexcept { return std::char_traits<char>::length(str); }

template <class T, std::enable_if_t<meta::has_method_data<const T&>, int> = 0>
constexpr auto data(const T& x) noexcept {
    return x.data();
}

constexpr auto data(const char* x) noexcept { return x; }

constexpr auto data(char* x) noexcept { return x; }

template <class T>
constexpr decltype(auto) stringify(T&& x) {
    if constexpr (std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>) {
        return ::to_string(x);
    } else {
        return std::forward<T>(x);
    }
}

namespace detail {

template <class T, class = decltype(string_length(stringify(std::forward<T>(std::declval<T>()))))>
constexpr auto is_string_argument(int) -> std::true_type;

template <class>
constexpr auto is_string_argument(...) -> std::false_type;

} // namespace detail

template <class T>
constexpr inline bool is_string_argument = decltype(detail::is_string_argument<T>(0))::value;
