#pragma once

#include <cassert>
#include <cstdio>
#include <limits>
#include <type_traits>

// Function useful to sanitize character before putting it into e.g.
// std::is_digit() or std::tolower()
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr int safe_char(T c) {
    if constexpr (std::is_same_v<T, char> or std::is_same_v<T, signed char>) {
        return static_cast<unsigned char>(c);
    } else if constexpr (std::is_same_v<T, unsigned char>) {
        return c;
    } else {
        int x = c;
        using uchar_limits = std::numeric_limits<unsigned char>;
        assert(x == EOF or (uchar_limits::min() <= x and x <= uchar_limits::max()));
        return x;
    }
}

// Standard functions reimplemented to be constexpr
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_digit(T c) noexcept {
    return ('0' <= c and c <= '9');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_alpha(T c) noexcept {
    return ('A' <= c and c <= 'Z') or ('a' <= c and c <= 'z');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_alnum(T c) noexcept {
    return is_alpha(c) or is_digit(c);
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_xdigit(T c) noexcept {
    return is_digit(c) or ('A' <= c and c <= 'F') or ('a' <= c and c <= 'f');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_lower(T c) noexcept {
    return ('a' <= c and c <= 'z');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_upper(T c) noexcept {
    return ('A' <= c and c <= 'Z');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_cntrl(T c) noexcept {
    return ('\0' <= c and c <= '\x1f') or (c == '\x7f');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_space(T c) noexcept {
    return (c == '\t') or ('\x0a' <= c and c <= '\x0d') or (c == ' ');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_blank(T c) noexcept {
    return (c == '\t') or (c == ' ');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_graph(T c) noexcept {
    return ('\x21' <= c and c <= '\x7e');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_print(T c) noexcept {
    return ('\x20' <= c and c <= '\x7e');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_punct(T c) noexcept {
    return ('\x21' <= c and c <= '\x2f') or ('\x3a' <= c and c <= '\x40') or
        ('\x5b' <= c and c <= '\x60') or ('\x7b' <= c and c <= '\x7e');
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr T to_lower(T c) noexcept {
    return (is_upper(c) ? 'a' + (c - 'A') : c);
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr T to_upper(T c) noexcept {
    return (is_lower(c) ? 'A' + (c - 'a') : c);
}
