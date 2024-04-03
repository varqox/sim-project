#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace meta {

template <char... chars>
constexpr inline char static_chars[] = {chars...};

template <class Func, size_t... I>
constexpr const char*
char_array_to_static_chars(Func func, std::index_sequence<I...> /**/) noexcept {
    constexpr auto arr = func();
    return static_chars<arr[I]...>;
}

constexpr inline size_t cstr_length(const char* str) noexcept {
    auto orig_str = str;
    while (*str) {
        ++str;
    }
    return str - orig_str;
}

template <const char*... str>
constexpr inline auto concated_to_cstr = [] {
    constexpr auto lam = [] {
        std::array<char, (cstr_length(str) + ... + 1)> res{};
        size_t idx = 0;
        auto append = [&res, &idx](const char* s) {
            while (*s) {
                res[idx++] = *s++;
            }
        };
        (append(str), ...);
        res[idx] = '\0';
        return res;
    };
    return char_array_to_static_chars(
        lam, std::make_index_sequence<(cstr_length(str) + ... + 1)>{}
    );
}();

} // namespace meta
