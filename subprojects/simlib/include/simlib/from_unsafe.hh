#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

template <class... T>
struct from_unsafe {
    std::tuple<T&&...> data;

    constexpr explicit from_unsafe(T&&... args) noexcept
    : data{std::forward_as_tuple(std::forward<T>(args)...)} {}

    from_unsafe(const from_unsafe&) = delete;
    from_unsafe(from_unsafe&&) = delete;
    from_unsafe& operator=(const from_unsafe&) = delete;
    from_unsafe& operator=(from_unsafe&&) = delete;
    ~from_unsafe() = default;

    template <size_t idx>
    constexpr auto&& get() noexcept {
        return std::move(std::get<idx>(data));
    }

    template <size_t idx>
    constexpr auto& get_ref() noexcept {
        return std::get<idx>(data);
    }
};

template <class... T>
from_unsafe(T&&... args) -> from_unsafe<T...>;
