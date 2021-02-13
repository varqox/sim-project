#pragma once

#include <algorithm>
#include <simlib/string_traits.hh>

inline bool is_username(StringView str) noexcept {
    return std::all_of(
        str.begin(), str.end(), [](int x) { return (is_alnum(x) || x == '_' || x == '-'); });
}
