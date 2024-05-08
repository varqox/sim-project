#pragma once

#include <type_traits>

template <class T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
auto enum_to_underlying_type(T enum_val) noexcept {
    return static_cast<std::underlying_type_t<T>>(enum_val);
}
