#pragma once

#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"

#include <cstdlib>
#include <optional>
#include <type_traits>

// Evaluates b and other arguments and concatenates b to the last one
#define REV_CAT(b, ...) REV_CAT_I(b, __VA_ARGS__)
#define REV_CAT_I(b, ...) __VA_ARGS__##b

// Example usage: ENUM_WITH_STRING_CONVERSIONS(Color, uint8_t, (GREEN, 1, "green")(RED, 2,
// "red")(BLUE, 42, "blue"));
#define ENUM_WITH_STRING_CONVERSIONS(enum_name, underlying_type, seq)                    \
    struct enum_name {                                                                   \
        struct enum_with_string_conversions_marker {};                                   \
        using UnderlyingType = underlying_type;                                          \
        enum class Enum : underlying_type {                                              \
            IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS(seq)                         \
        };                                                                               \
                                                                                         \
        IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS(seq)                           \
                                                                                         \
        /* NOLINTNEXTLINE */                                                             \
        constexpr enum_name(Enum x = {})                                                 \
        : val{x} {}                                                                      \
                                                                                         \
        constexpr explicit enum_name(UnderlyingType x)                                   \
        : val{x} {}                                                                      \
                                                                                         \
        constexpr operator Enum() const noexcept { return val; }                         \
                                                                                         \
        constexpr explicit operator underlying_type() const noexcept {                   \
            return static_cast<underlying_type>(val);                                    \
        }                                                                                \
                                                                                         \
        constexpr explicit operator bool() = delete;                                     \
                                                                                         \
        constexpr CStringView to_str() const noexcept {                                  \
            switch (val) {                                                               \
                IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES(seq)                      \
            }                                                                            \
            std::abort(); /* invalid enum variant */                                     \
        }                                                                                \
                                                                                         \
        friend constexpr CStringView to_str(enum_name x) noexcept { return x.to_str(); } \
                                                                                         \
        constexpr CStringView to_quoted_str() const noexcept {                           \
            switch (val) {                                                               \
                IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES(seq)               \
            }                                                                            \
            std::abort(); /* invalid enum variant */                                     \
        }                                                                                \
                                                                                         \
        friend constexpr CStringView to_quoted_str(enum_name x) noexcept {               \
            return x.to_quoted_str();                                                    \
        }                                                                                \
                                                                                         \
        static constexpr std::optional<enum_name> from_str(StringView str) noexcept {    \
            IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS(seq)                          \
            return std::nullopt;                                                         \
        }                                                                                \
                                                                                         \
    private:                                                                             \
        Enum val;                                                                        \
    }

#define IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS(seq) \
    REV_CAT(_END, IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_1 seq)
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_1_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_2_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_1(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_3(name, val)              \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_2
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_2(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_3(name, val)              \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_1
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_ENUM_VARIANTS_3(name, val) name = val,

#define IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS(seq) \
    REV_CAT(_END, IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_1 seq)
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_1_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_2_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_1(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_3(name)                   \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_2
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_2(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_3(name)                   \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_1
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_STATIC_VARIANTS_3(name) \
    static constexpr Enum name = Enum::name;

#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES(seq) \
    REV_CAT(_END, IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_1 seq)
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_1_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_2_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_1(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_3(name, str_val)          \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_2
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_2(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_3(name, str_val)          \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_1
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_STR_CASES_3(name, str_val) \
    case name:                                                          \
        return str_val;

#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES(seq) \
    REV_CAT(_END, IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_1 seq)
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_1_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_2_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_1(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_3(name, str_val)          \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_2
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_2(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_3(name, str_val)          \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_1
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_TO_QUOTED_STR_CASES_3(name, str_val) \
    case name:                                                                 \
        return "\"" str_val "\"";

#define IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS(seq) \
    REV_CAT(_END, IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_1 seq)
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_1_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_2_END
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_1(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_3(name, str_val)          \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_2
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_2(name, val, str_val) \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_3(name, str_val)          \
    IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_1
#define IMPL_ENUM_WITH_STRING_CONVERSIONS_FROM_STR_IFS_3(name, str_val) \
    if (str == str_val)                                                 \
        return name;

namespace detail {

template <class T, class = typename T::enum_with_string_conversions_marker>
constexpr auto has_enum_with_string_conversions_marker(int) -> std::true_type;
template <class>
constexpr auto has_enum_with_string_conversions_marker(...) -> std::false_type;

} // namespace detail

template <class T>
constexpr inline bool is_enum_with_string_conversions =
    decltype(detail::has_enum_with_string_conversions_marker<T>(0))::value;
