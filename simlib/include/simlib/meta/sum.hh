#pragma once

#include <cstdint>

namespace meta {

template <intmax_t x, intmax_t... ints>
constexpr inline intmax_t sum = x + sum<ints...>;

template <intmax_t x>
constexpr inline intmax_t sum<x> = x;

} // namespace meta
