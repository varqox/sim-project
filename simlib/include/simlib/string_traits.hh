#pragma once

#include "simlib/ctype.hh"
#include "simlib/string_view.hh"

constexpr bool has_prefix(const StringView& str, const StringView& prefix) noexcept {
    return (str.compare(0, prefix.size(), prefix) == 0);
}

template <class... T>
constexpr bool has_one_of_prefixes(StringView str, T&&... prefixes) noexcept {
    return (... or has_prefix(str, std::forward<T>(prefixes)));
}

constexpr bool has_suffix(const StringView& str, const StringView& suffix) noexcept {
    return (str.size() >= suffix.size() and
            str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

template <class... T>
constexpr bool has_one_of_suffixes(StringView str, T&&... suffixes) noexcept {
    return (... or has_suffix(str, std::forward<T>(suffixes)));
}

constexpr bool is_digit(const StringView& str) noexcept {
    if (str.empty()) {
        return false;
    }
    return std::all_of(str.begin(), str.end(), is_digit<char>);
}

constexpr bool is_alpha(const StringView& str) noexcept {
    if (str.empty()) {
        return false;
    }
    return std::all_of(str.begin(), str.end(), is_alpha<char>);
}

constexpr bool is_alnum(const StringView& str) noexcept {
    if (str.empty()) {
        return false;
    }
    return std::all_of(str.begin(), str.end(), is_alnum<char>);
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
constexpr bool is_word(T c) noexcept {
    return (is_alnum(c) or c == '_' or c == '-');
}

constexpr bool is_word(const StringView& str) noexcept {
    if (str.empty()) {
        return false;
    }
    return std::all_of(str.begin(), str.end(), is_word<char>);
}

/**
 * @brief Check whether string @p str is an integer
 * @details Equivalent to check id string matches regex \-?[0-9]+
 *   Notes:
 *   - empty string is not an integer
 *   - sign is not an integer
 *
 * @param str string
 *
 * @return result - whether string @p str is an integer
 */

constexpr bool is_integer(StringView str) noexcept {
    if (str.empty()) {
        return false;
    }

    if (str[0] == '-') {
        str.remove_prefix(1);
    }

    return is_digit(str);
}

constexpr bool is_real(StringView str) noexcept {
    if (str.empty()) {
        return false;
    }

    if (str.front() == '-') {
        str.remove_prefix(1);
    }

    size_t dot_pos = str.find('.');
    if (dot_pos >= str.size()) {
        return is_digit(str);
    }

    return (is_digit(str.substring(0, dot_pos)) and is_digit(str.substring(dot_pos + 1)));
}
