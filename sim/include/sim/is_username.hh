#pragma once

#include "simlib/string_traits.hh"

#include <algorithm>

namespace sim {

inline bool is_username(StringView str) noexcept {
    return std::all_of(
            str.begin(), str.end(), [](int x) { return (is_alnum(x) || x == '_' || x == '-'); });
}

} // namespace sim
