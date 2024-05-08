#pragma once

#include <cstddef>

namespace meta {

template <class T, class E>
constexpr size_t count(const T& container, const E& elem) noexcept {
    size_t res = 0;
    for (const auto& x : container) {
        res += x == elem;
    }
    return res;
}

constexpr size_t count(const char* str, char c) noexcept {
    size_t res = 0;
    while (*str) {
        res += *str == c;
        ++str;
    }
    return res;
}

} // namespace meta
