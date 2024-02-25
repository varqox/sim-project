#pragma once

#include <type_traits>
#include <utility>

namespace meta {

template <class T>
constexpr T max(T&& x) {
    return x;
}

template <class T, class U>
constexpr typename std::common_type<T, U>::type max(T&& x, U&& y) {
    using CT = typename std::common_type<T, U>::type;
    CT xx(std::forward<T>(x));
    CT yy(std::forward<U>(y));
    return (xx > yy ? xx : yy);
}

template <class T, class... Args>
constexpr typename std::common_type<T, Args...>::type max(T&& x, Args&&... args) {
    return max(std::forward<T>(x), max(std::forward<Args>(args)...));
}

} // namespace meta
