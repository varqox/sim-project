#pragma once

#include <type_traits>

namespace meta {

template <class T, class... Options>
constexpr inline bool is_one_of = (std::is_same_v<T, Options> or ...);

} // namespace meta
