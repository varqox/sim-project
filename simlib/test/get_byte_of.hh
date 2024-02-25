#pragma once

#include <cstddef>
#include <cstring>

template <size_t N, class T>
std::byte get_byte_of(const T& val) noexcept {
    static_assert(N < sizeof(T)); // NOLINT(bugprone-sizeof-expression)
    std::byte res;
    std::memcpy(&res, reinterpret_cast<const std::byte*>(&val) + N, 1);
    return res;
}
