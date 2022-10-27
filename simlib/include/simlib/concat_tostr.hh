#pragma once

#include <simlib/concat_common.hh>
#include <type_traits>

template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
std::string concat_tostr(Args&&... args) {
    return [](auto&&... str) {
        size_t total_length = (0 + ... + string_length(str));
        std::string res;
        res.reserve(total_length);
        (void)(res += ... += std::forward<decltype(str)>(str));
        return res;
    }(stringify(std::forward<Args>(args))...);
}

template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
std::string& back_insert(std::string& str, Args&&... args) {
    return [&str](auto&&... xx) -> std::string& {
        size_t total_length = (str.size() + ... + string_length(xx));
        // Allocate more space (reserve() is inefficient)
        size_t old_size = str.size();
        str.resize(total_length);
        str.resize(old_size);
        return (str += ... += std::forward<decltype(xx)>(xx));
    }(stringify(std::forward<Args>(args))...);
}
