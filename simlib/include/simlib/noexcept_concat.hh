#pragma once

#include <cstddef>
#include <simlib/to_string.hh>
#include <string>
#include <type_traits>

namespace detail {

template <class>
struct NoexceptStringMaxLength {};

template <size_t N>
struct NoexceptStringMaxLength<StaticCStringBuff<N>> : std::integral_constant<size_t, N> {};

template <size_t N>
struct NoexceptStringMaxLength<char[N]> : std::integral_constant<size_t, N - 1> {};

} // namespace detail

template <class T>
constexpr inline size_t noexcept_string_max_length =
        detail::NoexceptStringMaxLength<std::remove_cv_t<std::remove_reference_t<T>>>::value;

template <class T>
constexpr decltype(auto) noexcept_stringify(T&& x) noexcept {
    if constexpr (std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>) {
        return ::to_string(x);
    } else {
        return std::forward<T>(x);
    }
}

template <size_t N>
constexpr auto noexcept_string_length(const StaticCStringBuff<N>& str) noexcept {
    return str.size();
}

template <size_t N>
constexpr auto noexcept_string_length(const char (&/*str*/)[N]) noexcept {
    return N - 1;
}

template <size_t N>
constexpr auto noexcept_string_data(const StaticCStringBuff<N>& str) noexcept {
    return str.data();
}

template <size_t N>
constexpr auto noexcept_string_data(const char (&str)[N]) noexcept {
    return str;
}

namespace detail {

template <class T,
        class = decltype(noexcept_string_length(
                noexcept_stringify(std::forward<T>(std::declval<T>()))))>
constexpr auto is_noexcept_string_argument(int) -> std::true_type;

template <class>
constexpr auto is_noexcept_string_argument(...) -> std::false_type;

} // namespace detail

template <class T>
constexpr inline bool is_noexcept_string_argument =
        decltype(detail::is_noexcept_string_argument<T>(0))::value;

template <class... Args, std::enable_if_t<(is_noexcept_string_argument<Args> and ...), int> = 0>
[[nodiscard]] constexpr auto noexcept_concat(Args&&... args) noexcept {
    return [](auto&&... str) {
        StaticCStringBuff<(noexcept_string_max_length<decltype(str)> + ... + 0)> res;
        size_t pos = 0;
        auto append = [&](auto&& s) {
            auto slen = noexcept_string_length(s);
            auto sdata = noexcept_string_data(s);
            while (slen > 0) {
                --slen;
                res[pos++] = *(sdata++);
            }
        };
        (append(str), ...);
        res[pos] = '\0';
        res.len_ = pos;
        return res;
    }(noexcept_stringify(std::forward<Args>(args))...);
}
