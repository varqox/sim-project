#pragma once

#include <limits>
#include <type_traits>

template <
    class To,
    class From,
    std::enable_if_t<std::is_integral_v<From> && std::is_integral_v<To>, int> = 0>
constexpr bool converts_safely_to(From from) noexcept {
    if constexpr (std::is_signed_v<From> == std::is_signed_v<To>) {
        return std::numeric_limits<To>::min() <= from && from <= std::numeric_limits<To>::max();
    } else if constexpr (std::is_signed_v<From>) {
        if (from < From{0}) {
            return false;
        }
        auto unsigned_from = static_cast<std::make_unsigned_t<From>>(from);
        return unsigned_from <= std::numeric_limits<To>::max();
    } else {
        return from <= static_cast<std::make_unsigned_t<To>>(std::numeric_limits<To>::max());
    }
}
