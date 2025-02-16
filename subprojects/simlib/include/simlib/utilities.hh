#pragma once

#include <algorithm>
#include <functional>

template <class T, class... Args>
T& back_insert(T& reference, Args&&... args) {
    // Allocate more space (reserve is inefficient)
    size_t old_size = reference.size();
    reference.resize(old_size + sizeof...(args));
    reference.resize(old_size);

    (reference.emplace_back(std::forward<Args>(args)), ...);
    return reference;
}

template <class T, class C>
constexpr typename T::const_iterator binary_find(const T& x, const C& val) {
    auto beg = x.begin();
    auto end = x.end();
    while (beg != end) {
        auto mid = beg + ((end - beg) >> 1);
        if (*mid < val) {
            beg = ++mid;
        } else {
            end = mid;
        }
    }
    return (beg != x.end() && *beg == val ? beg : x.end());
}

template <class T, class C, class Comp>
constexpr typename T::const_iterator binary_find(const T& x, const C& val, Comp&& comp) {
    auto beg = x.begin();
    auto end = x.end();
    while (beg != end) {
        auto mid = beg + ((end - beg) >> 1);
        if (comp(*mid, val)) {
            beg = ++mid;
        } else {
            end = mid;
        }
    }
    return (beg != x.end() && !comp(*beg, val) && !comp(val, *beg) ? beg : x.end());
}

template <class T, typename B, class C>
constexpr typename T::const_iterator
binary_find_by(const T& x, B T::value_type::* field, const C& val) {
    auto beg = x.begin();
    auto end = x.end();
    while (beg != end) {
        auto mid = beg + ((end - beg) >> 1);
        if ((*mid).*field < val) {
            beg = ++mid;
        } else {
            end = mid;
        }
    }
    return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}

template <class T, typename B, class C, class Comp>
constexpr typename T::const_iterator
binary_find_by(const T& x, B T::value_type::* field, const C& val, Comp&& comp) {
    auto beg = x.begin();
    auto end = x.end();
    while (beg != end) {
        auto mid = beg + ((end - beg) >> 1);
        if (comp((*mid).*field, val)) {
            beg = ++mid;
        } else {
            end = mid;
        }
    }
    return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}

// Aliases - take container instead of range
template <class A, class B>
inline bool binary_search(const A& a, B&& val) {
    return std::binary_search(a.begin(), a.end(), std::forward<B>(val));
}

template <class A, class B>
inline auto lower_bound(const A& a, B&& val) -> decltype(a.begin()) {
    return std::lower_bound(a.begin(), a.end(), std::forward<B>(val));
}

template <class A, class B>
inline auto upper_bound(const A& a, B&& val) -> decltype(a.begin()) {
    return std::upper_bound(a.begin(), a.end(), std::forward<B>(val));
}

template <class A>
inline void sort(A& a) {
    std::sort(a.begin(), a.end());
}

template <class A, class Func>
inline void sort(A& a, Func&& func) {
    std::sort(a.begin(), a.end(), std::forward<Func>(func));
}

template <class T>
constexpr bool is_sorted(const T& collection) {
    auto b = begin(collection);
    auto e = end(collection);
    if (b == e) {
        return true;
    }

    auto prev = b;
    while (++b != e) {
        if (std::less<>()(*b, *prev)) {
            return false;
        }

        prev = b;
    }

    return true;
}

template <class A, class... Option>
constexpr bool is_one_of(A&& val, Option&&... option) {
    return ((val == option) or ...);
}

template <class...>
constexpr inline bool is_pair = false;

template <class A, class B>
constexpr inline bool is_pair<std::pair<A, B>> = true;

// Be aware that it will be slow for containers like std::string or std::vector
// where erase() is slow (use std::remove_if() there), but for std::set or
// std::map it is fast
template <class Container, class Predicate>
Container filter(Container&& container, Predicate&& pred) {
    auto it = container.begin();
    while (it != container.end()) {
        if (not pred(*it)) {
            container.erase(it++);
        } else {
            ++it;
        }
    }

    return std::forward<Container>(container);
}
