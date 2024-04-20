#pragma once

#include <cstdlib>
#include <optional>
#include <simlib/always_false.hh>
#include <simlib/ctype.hh>
#include <simlib/inplace_buff.hh>

inline std::string to_lower(std::string str) {
    for (auto& c : str) {
        c = to_lower(c);
    }
    return str;
}

template <class T>
constexpr unsigned char hex2dec(T c) noexcept {
    assert(is_xdigit(c));
    return (c < 'A' ? c - '0' : 10 + c - (c >= 'a' ? 'a' : 'A'));
}

constexpr char dec2hex(int x) noexcept {
    assert(0 <= x and x <= 15);
    return static_cast<char>(x > 9 ? 'a' - 10 + x : x + '0');
}

constexpr char dec2Hex(int x) noexcept {
    assert(0 <= x and x <= 15);
    return static_cast<char>(x > 9 ? 'A' - 10 + x : x + '0');
}

/// Converts each byte of @p str to two hex digits using dec2hex()
template <size_t N = 64>
InplaceBuff<N> to_hex(const StringView& str) {
    InplaceBuff<N> res(str.size() << 1);
    size_t i = 0;
    for (unsigned char c : str) {
        res[i++] = dec2hex(c >> 4);
        res[i++] = dec2hex(c & 15);
    }

    res.size = i;
    return res;
}

// Encodes as URI
template <size_t N = 256>
InplaceBuff<N> encode_uri(const StringView& str) {
    using std::array;
    static constexpr auto is_safe = [] {
        array<bool, 256> res = {};
        for (auto [beg, end] : array{array{'a', 'z'}, array{'A', 'Z'}, array{'0', '9'}}) {
            for (unsigned char i = beg; i <= end; ++i) {
                res[i] = true;
            }
        }

        for (unsigned char c : StringView("-_.~")) {
            res[c] = true;
        }

        return res;
    }();

    static_assert(is_safe['a']);
    static_assert(is_safe['z']);
    static_assert(is_safe['A']);
    static_assert(is_safe['Z']);
    static_assert(is_safe['0']);
    static_assert(is_safe['9']);
    static_assert(is_safe['-']);
    static_assert(is_safe['_']);
    static_assert(is_safe['.']);
    static_assert(is_safe['~']);
    static_assert(not is_safe['\0']);

    InplaceBuff<N> res;
    for (unsigned char c : str) {
        if (is_safe[c]) {
            res.append(c);
        } else {
            res.append('%', dec2Hex(c >> 4), dec2Hex(c & 15));
        }
    }

    return res;
}

/// Decodes URI
template <size_t N = 256>
InplaceBuff<N> decode_uri(StringView str) {
    InplaceBuff<N> res;
    for (size_t i = 0; i < str.size(); ++i) {
        char c = 0;
        if (str[i] == '%' and i + 2 < str.size() and is_xdigit(str[i + 1]) and
            is_xdigit(str[i + 2]))
        {
            c = static_cast<char>(
                static_cast<unsigned char>((hex2dec(str[i + 1]) << 4) | hex2dec(str[i + 2]))
            );
            i += 2;
        } else if (str[i] == '+') {
            c = ' ';
        } else {
            c = str[i];
        }

        res.append(c);
    }

    return res;
}

// Escapes HTML unsafe character and appends it to @p str
inline void append_as_html_escaped(std::string& str, char c) {
    switch (c) {
    // To preserve spaces use CSS: white-space: pre | pre-wrap;
    case '&': str += "&amp;"; break;
    case '"': str += "&quot;"; break;
    case '\'': str += "&apos;"; break;
    case '<': str += "&lt;"; break;
    case '>': str += "&gt;"; break;
    default: str += c;
    }
}

// Escapes HTML unsafe character sequences and appends them to @p str
inline void append_as_html_escaped(std::string& str, const StringView& s) {
    for (auto c : s) {
        append_as_html_escaped(str, c);
    }
}

// Escapes HTML unsafe character sequences
template <class T>
inline std::string html_escape(const T& str) {
    std::string res;
    append_as_html_escaped(res, str);
    return res;
}

template <class... Args, std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
void append_stringified_json(std::string& str, Args&&... args) {
    auto safe_append = [&str](auto&& arg) {
        auto p = ::data(arg);
        for (size_t i = 0, len = string_length(arg); i < len; ++i) {
            unsigned char c = p[i];
            if (c == '\"') {
                str += "\\\"";
            } else if (c == '\n') {
                str += "\\n";
            } else if (c == '\\') {
                str += "\\\\";
            } else if (is_cntrl(c)) {
                str += "\\u00";
                str += dec2hex(c >> 4);
                str += dec2hex(c & 15);
            } else {
                str += static_cast<char>(c);
            }
        }
    };

    str += '"';
    (safe_append(stringify(std::forward<Args>(args))), ...);
    str += '"';
}

template <class... Arg, std::enable_if_t<(is_string_argument<Arg> and ...), int> = 0>
std::string json_stringify(Arg&&... arg) {
    std::string res;
    append_stringified_json(res, std::forward<Arg>(arg)...);
    return res;
}

// Converts whole @p str to @p T or returns std::nullopt on errors like value
// represented in @p str is too big or invalid
template <
    class T,
    std::enable_if_t<std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>, int> = 0>
constexpr std::optional<T> str2num(StringView str) noexcept {
    if constexpr (std::is_same_v<T, bool>) {
        auto opt = str2num<uint8_t>(str);
        if (opt and *opt <= 1) {
            return *opt;
        }

        return std::nullopt;
    } else {
        if (str.empty()) {
            return std::nullopt;
        }

        bool minus = false;
        if (not std::is_unsigned_v<T> and str[0] == '-') {
            minus = true;
            str.remove_prefix(1);
            if (str.empty()) {
                return std::nullopt;
            }
        }

        if (not is_digit(str[0])) {
            return std::nullopt;
        }

        std::optional<T> res = (minus ? '0' - str[0] : str[0] - '0'); // Will not overflow
        str.remove_prefix(1);

        for (unsigned char c : str) {
            if (not is_digit(c)) {
                return std::nullopt;
            }

            if (__builtin_mul_overflow(*res, 10, &*res)) {
                return std::nullopt;
            }

            if (__builtin_add_overflow(*res, (minus ? '0' - c : c - '0'), &*res)) {
                return std::nullopt;
            }
        }

        return res;
    }
}

// Converts whole @p str to @p T or returns std::nullopt on errors like value
// represented in @p str is too big or invalid
template <
    class T,
    std::enable_if_t<not std::is_integral_v<std::remove_cv_t<std::remove_reference_t<T>>>, int> = 0>
std::optional<T> str2num(StringView str) noexcept {
#if 0 // TODO: use it when std::from_chars for double becomes implemented in libstdc++ (also
      // remove always false from the above and include <cstdlib>)
    std::optional<T> res{std::in_place};
    auto [ptr, ec] = std::from_chars(str.begin(), str.end(), &*res);
    if (ptr != str.end() or ec != std::errc())
        return std::nullopt;

    return res;
#else
    static_assert(std::is_floating_point_v<T>);
    if (str.empty() or is_space(str[0])) {
        return std::nullopt;
    }

    try {
        InplaceBuff<4096> buff{str};
        CStringView cstr = buff.to_cstr();
        auto err = std::exchange(errno, 0);

        char* ptr = nullptr;
        T res = [&] {
            if constexpr (std::is_same_v<T, float>) {
                return ::strtof(cstr.data(), &ptr);
            } else if constexpr (std::is_same_v<T, double>) {
                return ::strtod(cstr.data(), &ptr);
            } else if constexpr (std::is_same_v<T, long double>) {
                return ::strtold(cstr.data(), &ptr);
            } else {
                static_assert(always_false<T>, "Cannot convert this type");
            }
        }();

        std::swap(err, errno);
        if (err or ptr != cstr.end()) {
            return std::nullopt;
        }

        return res;

    } catch (...) {
        return std::nullopt;
    }

#endif
}

// Converts whole @p str to @p T or returns std::nullopt on errors like value
// represented in @p str is too big, invalid or not in range [@p min_val,
// @p max_val]
template <class T>
constexpr std::optional<T> str2num(StringView str, T min_val, T max_val) noexcept {
    auto res = str2num<T>(str);
    if (not res or *res < min_val or *res > max_val) {
        return std::nullopt;
    }

    return res;
}

enum Adjustment : uint8_t { LEFT, RIGHT };

/**
 * @brief Returns a padded string
 * @details Examples:
 *   padded_string("abc", 5) -> "  abc"
 *   padded_string("abc", 5, LEFT) -> "abc  "
 *   padded_string("1234", 7, RIGHT, '0') -> "0001234"
 *   padded_string("1234", 4, RIGHT, '0') -> "1234"
 *   padded_string("1234", 2, RIGHT, '0') -> "1234"
 *
 * @param str string to pad
 * @param len minimum length of result string
 * @param adj adjustment direction to left or right
 * @param filler character used to fill blank fields
 *
 * @return formatted string
 */
template <size_t N = 32>
InplaceBuff<N>
padded_string(StringView str, size_t len, Adjustment adj = RIGHT, char filler = ' ') {
    if (len <= str.size()) {
        return InplaceBuff<N>(str);
    }

    if (adj == LEFT) {
        auto res = InplaceBuff<N>(str);
        res.resize(len);
        std::fill(res.begin() + str.size(), res.end(), filler);
        return res;
    }

    InplaceBuff<N> res{len - str.size()};
    std::fill(res.begin(), res.end(), filler);
    res.append(str);
    return res;
}
