#pragma once

#include <cstddef>
#include <iterator>

template <class T>
class Slice {
    T* begin_;
    T* end_;

public:
    [[nodiscard]] constexpr Slice() noexcept : begin_{nullptr}, end_{nullptr} {}

    template <size_t N>
    // NOLINTNEXTLINE(google-explicit-constructor)
    [[nodiscard]] constexpr Slice(T (&&arr)[N]) noexcept : begin_{arr}
                                                         , end_{arr + N} {}

    [[nodiscard]] constexpr Slice(T* data, size_t size) noexcept
    : begin_{data}
    , end_{data + size} {}

    template <class Container>
    // NOLINTNEXTLINE(google-explicit-constructor,bugprone-forwarding-reference-overload)
    [[nodiscard]] constexpr Slice(Container&& container)
    : Slice(std::data(container), std::size(container)) {}

    [[nodiscard]] constexpr size_t size() const noexcept { return end_ - begin_; }

    [[nodiscard]] constexpr bool is_empty() const noexcept { return size() == 0; }

    [[nodiscard]] constexpr T* data() noexcept { return begin_; }

    [[nodiscard]] constexpr const T* data() const noexcept { return begin_; }

    [[nodiscard]] constexpr T* begin() noexcept { return begin_; }

    [[nodiscard]] constexpr const T* begin() const noexcept { return begin_; }

    [[nodiscard]] constexpr T* end() noexcept { return end_; }

    [[nodiscard]] constexpr const T* end() const noexcept { return end_; }

    [[nodiscard]] constexpr T& operator[](size_t n) noexcept { return begin()[n]; }

    [[nodiscard]] constexpr const T& operator[](size_t n) const noexcept { return begin()[n]; }
};
