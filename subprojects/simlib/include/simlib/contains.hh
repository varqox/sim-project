#pragma once

template <class Collection, class Elem>
constexpr bool contains(const Collection& collection, const Elem& elem) {
    for (const auto& e : collection) {
        if (e == elem) {
            return true;
        }
    }
    return false;
}
