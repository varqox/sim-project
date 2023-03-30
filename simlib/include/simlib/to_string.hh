#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <limits>
#include <string>

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

private:
    template <size_t M>
    struct Number {};

public:
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
        assert(*other.end() == '\0');
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

template <
    class T,
    std::enable_if_t<std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>, int> = 0>
constexpr auto to_string(T x) noexcept {
    constexpr auto digits = [](auto val) constexpr {
        size_t res = 0;
        while (val > 0) {
            ++res;
            val /= 10;
        }

        return res;
    };

    StaticCStringBuff<digits(std::numeric_limits<T>::max()) + 1> res;
    if (x == 0) {
        res[0] = '0';
        res[1] = '\0';
        res.len_ = 1;
        return res;
    }

    if (std::is_signed_v<T> and x < 0) {
        while (x) {
            auto x2 = x / 10;
            res[res.len_++] = x2 * 10 - x + '0';
            x = x2;
        }
        res[res.len_++] = '-';
    } else {
        while (x) {
            auto x2 = x / 10;
            res[res.len_++] = x - x2 * 10 + '0';
            x = x2;
        }
    }

    res[res.len_] = '\0';
#if __cplusplus > 201703L
#warning "std::reverse() can be used as it became constexpr"
#endif
    // Reverse res
    for (int i = 0, j = res.len_ - 1; i < j; ++i, --j) {
        char c = res[i];
        res[i] = res[j];
        res[j] = c;
    }

    return res;
}

constexpr auto to_string(char c) noexcept {
    // Need to explicitly provide size to work with c == '\0'
    return StaticCStringBuff<1>({c, '\0'}, 1);
}

// If you want to convert bool to 0 or 1, use to_string<int>()
constexpr auto to_string(bool x) noexcept {
    if (x) {
        return StaticCStringBuff<5>("true");
    }

    return StaticCStringBuff<5>("false");
}

template <
    class T,
    std::enable_if_t<not std::is_arithmetic_v<std::remove_cv_t<std::remove_reference_t<T>>>, int> =
        0>
std::string to_string(T x) {
    const auto zero = std::remove_cv_t<std::remove_reference_t<T>>();
    if (x == zero) {
        return {'0'};
    }

    std::string res;
    if (x < zero) {
        while (x) {
            auto x2 = x / 10;
            res += static_cast<char>(x2 * 10 - x + '0');
            x = x2;
        }
        res += '-';
    } else {
        while (x) {
            auto x2 = x / 10;
            res += static_cast<char>(x - x2 * 10 + '0');
            x = x2;
        }
    }

    std::reverse(res.begin(), res.end());
    return res;
}

template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
std::string to_string(T x, int precision = 6) {
    constexpr int STEP = 20;
#if 0 // TODO: use it when std::to_chars for double becomes implemented in libstdc++
    std::string res;
    size_t len = STEP;
    for (;;) {
        res.resize(len);
        auto [ptr, ec] =
            std::to_chars(res.data(), res.data() + len, x, std::chars_format::fixed);
        if (ec == std::errc()) {
            res.resize(ptr - res.data());
            return res;
        }

        len *= STEP;
    }
#else
    std::string res(STEP, '\0');
    int len = std::snprintf(res.data(), STEP - 1, "%.*Lf", precision, static_cast<long double>(x));
    if (len >= STEP) {
        res.resize(len + 1, '\0');
        (void
        )std::snprintf(res.data(), res.size() - 1, "%.*Lf", precision, static_cast<long double>(x));
    }

    res.resize(len);
    return res;
#endif
}
