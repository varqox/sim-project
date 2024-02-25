#pragma once

#include <type_traits>

#define DECLARE_ENUM_OPERATOR(enu, oper)                                     \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                         \
    constexpr enu operator oper(enu a, enu b) {                              \
        using UT = std::underlying_type<enu>::type;                          \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                     \
        return static_cast<enu>(static_cast<UT>(a) oper static_cast<UT>(b)); \
    }

#define DECLARE_ENUM_ASSIGN_OPERATOR(enu, oper)                   \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */              \
    constexpr enu& operator oper(enu& a, enu b) {                 \
        using UT = std::underlying_type<enu>::type;               \
        UT x = static_cast<UT>(a);                                \
        return (a = static_cast<enu>(x oper static_cast<UT>(b))); \
    }

#define DECLARE_ENUM_UNARY_OPERATOR(enu, oper)            \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */      \
    constexpr enu operator oper(enu a) {                  \
        using UT = std::underlying_type<enu>::type;       \
        /* NOLINTNEXTLINE(bugprone-macro-parentheses) */  \
        return static_cast<enu>(oper static_cast<UT>(a)); \
    }

#define DECLARE_ENUM_COMPARE1(enu, oper)                                     \
    constexpr bool operator oper(enu a, std::underlying_type<enu>::type b) { \
        return static_cast<decltype(b)>(a) oper b;                           \
    }

#define DECLARE_ENUM_COMPARE2(enu, oper)                                     \
    constexpr bool operator oper(std::underlying_type<enu>::type a, enu b) { \
        return a oper static_cast<decltype(a)>(b);                           \
    }
