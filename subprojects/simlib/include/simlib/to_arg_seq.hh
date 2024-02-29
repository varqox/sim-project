#pragma once

#include <string_view>

struct ArgSeq {
    char** beg_;
    char** end_;

    struct Iter {
        char** ptr;

        using difference_type = size_t;
        using value_type = std::string_view;
        using pointer = void;
        using reference = std::string_view;
        using iterator_category = std::input_iterator_tag;

        [[nodiscard]] constexpr std::string_view operator*() const noexcept {
            return std::string_view{*ptr};
        }

        constexpr Iter& operator++() noexcept {
            ++ptr;
            return *this;
        }

        [[nodiscard]] constexpr Iter operator++(int) noexcept { return Iter{.ptr = ptr++}; }

        [[nodiscard]] constexpr friend bool operator==(Iter a, Iter b) noexcept {
            return a.ptr == b.ptr;
        }

        [[nodiscard]] constexpr friend bool operator!=(Iter a, Iter b) noexcept {
            return a.ptr != b.ptr;
        }
    };

    [[nodiscard]] constexpr Iter begin() const noexcept { return {.ptr = beg_}; }

    [[nodiscard]] constexpr Iter end() const noexcept { return {.ptr = end_}; }
};

[[nodiscard]] constexpr ArgSeq to_arg_seq(int argc, char** argv) noexcept {
    // Skip program name
    if (argc > 0) {
        --argc;
        ++argv;
    }
    return ArgSeq{.beg_ = argv, .end_ = argv + argc};
}
