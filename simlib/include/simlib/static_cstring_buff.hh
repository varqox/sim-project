#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>

template <size_t N>
class StaticCStringBuff {
private:
    std::array<char, N + 1> str_{'\0'};

public:
    size_t len_ = 0;

    StaticCStringBuff() = default;

    constexpr StaticCStringBuff(const std::array<char, N + 1>& str, size_t len)
    : str_(str)
    , len_(len) {}

    explicit constexpr StaticCStringBuff(const std::array<char, N + 1>& str)
    : StaticCStringBuff(str, std::char_traits<char>::length(str.data())) {}

    template <size_t M, std::enable_if_t<M <= N + 1, int> = 0>
    explicit constexpr StaticCStringBuff(const char (&str)[M]) {
        while (len_ < M - 1) {
            str_[len_] = str[len_];
            ++len_;
        }
        str_[len_] = '\0';
    }

    StaticCStringBuff(const StaticCStringBuff&) noexcept = default;
    StaticCStringBuff(StaticCStringBuff&&) noexcept = default;

    template <size_t M, std::enable_if_t<M <= N, int> = 0>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr StaticCStringBuff(const StaticCStringBuff<M>& other) noexcept : len_(other.len_) {
        for (size_t i = 0; i < len_; ++i) {
            str_[i] = other[i];
        }
        str_[len_] = '\0';
    }

    StaticCStringBuff& operator=(const StaticCStringBuff&) noexcept = default;

    StaticCStringBuff& operator=(StaticCStringBuff&&) noexcept = default;

    ~StaticCStringBuff() = default;

    [[nodiscard]] constexpr bool is_empty() const noexcept { return len_ == 0; }

    [[nodiscard]] constexpr size_t size() const noexcept { return len_; }

    [[nodiscard]] static constexpr size_t max_size() noexcept { return N; }

    constexpr char* data() noexcept { return str_.data(); }

    [[nodiscard]] constexpr const char* data() const noexcept { return str_.data(); }

    [[nodiscard]] constexpr const char* c_str() const noexcept { return data(); }

    // NOLINTNEXTLINE(google-explicit-constructor)
    [[nodiscard]] constexpr operator std::string_view() const noexcept { return {data(), size()}; }

    constexpr auto begin() noexcept { return str_.begin(); }

    [[nodiscard]] constexpr auto begin() const noexcept { return str_.begin(); }

    constexpr auto end() noexcept { return begin() + len_; }

    [[nodiscard]] constexpr auto end() const noexcept { return begin() + len_; }

    constexpr char& operator[](size_t n) noexcept { return str_[n]; }

    constexpr const char& operator[](size_t n) const noexcept { return str_[n]; }

    template <size_t M>
    constexpr bool operator==(const char (&other)[M]) noexcept {
        if (len_ != M - 1) {
            return false;
        }
        return std::char_traits<char>::compare(data(), other, M) == 0;
    }
};

template <size_t N>
StaticCStringBuff(const char (&str)[N]) -> StaticCStringBuff<N>;
template <size_t N>
StaticCStringBuff(char (&str)[N]) -> StaticCStringBuff<N>;

template <size_t N>
std::string& operator+=(std::string& str, const StaticCStringBuff<N>& c_buff) {
    return str.append(c_buff.data(), c_buff.size());
}
