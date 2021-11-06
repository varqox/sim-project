#include "src/web_server/http/form_validation.hh"
#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/bool.hh"
#include "sim/sql_fields/varbinary.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/enum_with_string_conversions.hh"
#include "simlib/random_bytes.hh"
#include "simlib/ranges.hh"
#include "simlib/result.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/form_fields.hh"

#include <cassert>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

using std::nullopt;
using std::optional;
using std::string;
using std::string_view;
using web_server::http::ApiParam;
using web_server::http::FormFields;

namespace {

class ValidationTest {
    FormFields ff;

public:
    ValidationTest() = default;

    template <class T, class ResT = T>
    Result<ResT, string> validate(optional<string> form_field_value) {
        ff = FormFields{};
        if (form_field_value) {
            ff.add_field("abc", std::move(*form_field_value));
        }
        constexpr ApiParam<T> param_abc{"abc", "ABC"};
        VALIDATE(ff, [&](auto&& errors) { return Err{errors}; },
            (abc, param_abc)
        );
        static_assert(std::is_same_v<decltype(abc), ResT>);
        return Ok{abc};
    }

    template <class T, class ResT = T>
    Result<ResT, string> validate_allow_blank(optional<string> form_field_value) {
        ff = FormFields{};
        if (form_field_value) {
            ff.add_field("abc", std::move(*form_field_value));
        }
        constexpr ApiParam<T> param_abc{"abc", "ABC"};
        VALIDATE(ff, [&](auto&& errors) { return Err{errors}; },
            (abc, param_abc, ALLOW_BLANK)
        );
        static_assert(std::is_same_v<decltype(abc), ResT>);
        return Ok{abc};
    }

    template <class T, class ResT = T>
    Result<optional<ResT>, string>
    validate_allow_if(optional<string> form_field_value, bool condition) {
        ff = FormFields{};
        if (form_field_value) {
            ff.add_field("abc", std::move(*form_field_value));
        }
        constexpr ApiParam<T> param_abc{"abc", "ABC"};
        VALIDATE(ff, [&](auto&& errors) { return Err{errors}; },
            (abc, param_abc, ALLOW_IF(condition))
        );
        static_assert(std::is_same_v<decltype(abc), optional<ResT>>);
        return Ok{abc};
    }

    template <class T, class ResT = T>
    Result<optional<ResT>, string>
    validate_allow_blank_allow_if(optional<string> form_field_value, bool condition) {
        ff = FormFields{};
        if (form_field_value) {
            ff.add_field("abc", std::move(*form_field_value));
        }
        constexpr ApiParam<T> param_abc{"abc", "ABC"};
        VALIDATE(ff, [&](auto&& errors) { return Err{errors}; },
            (abc, param_abc, ALLOW_BLANK_ALLOW_IF(condition))
        );
        static_assert(std::is_same_v<decltype(abc), optional<ResT>>);
        return Ok{abc};
    }
};

} // namespace

constexpr size_t test_str_default_max_len = 70'000;

template <class StrType>
static void test_str(size_t max_len = test_str_default_max_len) {
    ValidationTest t;
    assert(max_len >= strlen("test value"));
    EXPECT_EQ(t.validate<StrType>("test value"), Ok{"test value"});
    EXPECT_EQ(t.validate<StrType>(""), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate<StrType>(nullopt), Err{"abc: ABC is not set"});

    auto str = random_bytes(max_len);
    EXPECT_EQ(t.validate<StrType>(str), Ok{str});
}

template <class StrType>
static void test_str_allow_blank(size_t max_len = test_str_default_max_len) {
    ValidationTest t;
    assert(max_len >= strlen("test value"));
    EXPECT_EQ(t.validate_allow_blank<StrType>("test value"), Ok{"test value"});
    EXPECT_EQ(t.validate_allow_blank<StrType>(""), Ok{""});
    EXPECT_EQ(t.validate_allow_blank<StrType>(nullopt), Err{"abc: ABC is not set"});

    auto str = random_bytes(max_len);
    EXPECT_EQ(t.validate<StrType>(str), Ok{str});
}

template <class StrType>
static void test_str_allow_if(size_t max_len = test_str_default_max_len) {
    ValidationTest t;
    assert(max_len >= strlen("test value"));
    EXPECT_EQ(t.validate_allow_if<StrType>("test value", true), Ok{"test value"});
    EXPECT_EQ(t.validate_allow_if<StrType>("", true), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate_allow_if<StrType>(nullopt, true), Err{"abc: ABC is not set"});
    EXPECT_EQ(
        t.validate_allow_if<StrType>("test value", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<StrType>("", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(t.validate_allow_if<StrType>(nullopt, false), Ok{nullopt});

    auto str = random_bytes(max_len);
    EXPECT_EQ(t.validate_allow_if<StrType>(str, true), Ok{str});
    EXPECT_EQ(
        t.validate_allow_if<StrType>(str, false),
        Err{"abc: ABC should not be sent within request at all"});
}

template <class StrType>
static void test_str_allow_blank_allow_if(size_t max_len = test_str_default_max_len) {
    ValidationTest t;
    assert(max_len >= strlen("test value"));
    EXPECT_EQ(t.validate_allow_blank_allow_if<StrType>("test value", true), Ok{"test value"});
    EXPECT_EQ(t.validate_allow_blank_allow_if<StrType>("", true), Ok{""});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<StrType>(nullopt, true), Err{"abc: ABC is not set"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<StrType>("test value", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<StrType>("", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(t.validate_allow_blank_allow_if<StrType>(nullopt, false), Ok{nullopt});

    auto str = random_bytes(max_len);
    EXPECT_EQ(t.validate_allow_if<StrType>(str, true), Ok{str});
    EXPECT_EQ(
        t.validate_allow_if<StrType>(str, false),
        Err{"abc: ABC should not be sent within request at all"});
}

// NOLINTNEXTLINE
TEST(form_validation, cstring_view) { test_str<CStringView>(); }

// NOLINTNEXTLINE
TEST(form_validation, cstring_view_allow_blank) { test_str_allow_blank<CStringView>(); }

// NOLINTNEXTLINE
TEST(form_validation, cstring_view_allow_if) { test_str_allow_if<CStringView>(); }

// NOLINTNEXTLINE
TEST(form_validation, cstring_view_allow_blank_allow_if) {
    test_str_allow_blank_allow_if<CStringView>();
}

// NOLINTNEXTLINE
TEST(form_validation, sql_blob) { test_str<sim::sql_fields::Blob<>>(); }

// NOLINTNEXTLINE
TEST(form_validation, sql_blob_allow_blank) {
    test_str_allow_blank<sim::sql_fields::Blob<>>();
}

// NOLINTNEXTLINE
TEST(form_validation, sql_blob_allow_if) { test_str_allow_if<sim::sql_fields::Blob<>>(); }

// NOLINTNEXTLINE
TEST(form_validation, sql_blob_allow_blank_allow_if) {
    test_str_allow_blank_allow_if<sim::sql_fields::Blob<>>();
}

// NOLINTNEXTLINE
TEST(form_validation, sql_varbinary) {
    ValidationTest t;
    constexpr size_t max_len = 14;
    using T = sim::sql_fields::Varbinary<max_len>;
    test_str<T>(max_len);

    for (size_t len = 1; len < max_len + 5; ++len) {
        auto str = random_bytes(len);
        using RT = Result<string, string>;
        EXPECT_EQ(
            t.validate<T>(str),
            len <= max_len
                ? RT{Ok{str}}
                : RT{Err{concat_tostr("abc: ABC cannot be longer than ", max_len, " bytes")}});
    }
}

// NOLINTNEXTLINE
TEST(form_validation, sql_varbinary_allow_blank) {
    ValidationTest t;
    constexpr size_t max_len = 14;
    using T = sim::sql_fields::Varbinary<max_len>;
    test_str_allow_blank<T>(max_len);

    for (size_t len = 1; len < max_len + 5; ++len) {
        auto str = random_bytes(len);
        using RT = Result<string, string>;
        EXPECT_EQ(
            t.validate_allow_blank<T>(str),
            len <= max_len
                ? RT{Ok{str}}
                : RT{Err{concat_tostr("abc: ABC cannot be longer than ", max_len, " bytes")}});
    }
}

// NOLINTNEXTLINE
TEST(form_validation, sql_varbinary_allow_if) {
    ValidationTest t;
    constexpr size_t max_len = 14;
    using T = sim::sql_fields::Varbinary<max_len>;
    test_str_allow_if<T>(max_len);

    for (size_t len = 1; len < max_len + 5; ++len) {
        auto str = random_bytes(len);
        using RT = Result<string, string>;
        EXPECT_EQ(
            t.validate_allow_if<T>(str, true),
            len <= max_len
                ? RT{Ok{str}}
                : RT{Err{concat_tostr("abc: ABC cannot be longer than ", max_len, " bytes")}});
        EXPECT_EQ(
            t.validate_allow_if<T>(str, false),
            Err{concat_tostr("abc: ABC should not be sent within request at all")});
    }
}

// NOLINTNEXTLINE
TEST(form_validation, sql_varbinary_allow_blank_allow_if) {
    ValidationTest t;
    constexpr size_t max_len = 14;
    using T = sim::sql_fields::Varbinary<max_len>;
    test_str_allow_blank_allow_if<T>(max_len);

    for (size_t len = 1; len < max_len + 5; ++len) {
        auto str = random_bytes(len);
        using RT = Result<string, string>;
        EXPECT_EQ(
            t.validate_allow_blank_allow_if<T>(str, true),
            len <= max_len
                ? RT{Ok{str}}
                : RT{Err{concat_tostr("abc: ABC cannot be longer than ", max_len, " bytes")}});
        EXPECT_EQ(
            t.validate_allow_blank_allow_if<T>(str, false),
            Err{concat_tostr("abc: ABC should not be sent within request at all")});
    }
}

template <class Bool, class ResT = Bool>
static void test_bool() {
    ValidationTest t;
    EXPECT_EQ((t.validate<Bool, ResT>("true")), Ok{true});
    EXPECT_EQ((t.validate<Bool, ResT>("false")), Ok{false});
    EXPECT_EQ((t.validate<Bool, ResT>("on")), Err{"abc: ABC has invalid value"});
    EXPECT_EQ((t.validate<Bool, ResT>("")), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ((t.validate<Bool, ResT>(nullopt)), Err{"abc: ABC is not set"});
}

template <class Bool, class ResT = Bool>
static void test_bool_allow_blank() {
    ValidationTest t;
    EXPECT_EQ((t.validate_allow_blank<Bool, ResT>("true")), Ok{true});
    EXPECT_EQ((t.validate_allow_blank<Bool, ResT>("false")), Ok{false});
    EXPECT_EQ((t.validate_allow_blank<Bool, ResT>("on")), Err{"abc: ABC has invalid value"});
    EXPECT_EQ((t.validate_allow_blank<Bool, ResT>("")), Err{"abc: ABC has invalid value"});
    EXPECT_EQ((t.validate_allow_blank<Bool, ResT>(nullopt)), Err{"abc: ABC is not set"});
}

template <class Bool, class ResT = Bool>
static void test_bool_allow_if() {
    ValidationTest t;
    EXPECT_EQ((t.validate_allow_if<Bool, ResT>("true", true)), Ok{true});
    EXPECT_EQ((t.validate_allow_if<Bool, ResT>("false", true)), Ok{false});
    EXPECT_EQ(
        (t.validate_allow_if<Bool, ResT>("on", true)), Err{"abc: ABC has invalid value"});
    EXPECT_EQ((t.validate_allow_if<Bool, ResT>("", true)), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ((t.validate_allow_if<Bool, ResT>(nullopt, true)), Err{"abc: ABC is not set"});
    EXPECT_EQ(
        (t.validate_allow_if<Bool, ResT>("true", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        (t.validate_allow_if<Bool, ResT>("false", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        (t.validate_allow_if<Bool, ResT>("on", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        (t.validate_allow_if<Bool, ResT>("", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ((t.validate_allow_if<Bool, ResT>(nullopt, false)), Ok{nullopt});
}

template <class Bool, class ResT = Bool>
static void test_bool_allow_blank_allow_if() {
    ValidationTest t;
    EXPECT_EQ((t.validate_allow_blank_allow_if<Bool, ResT>("true", true)), Ok{true});
    EXPECT_EQ((t.validate_allow_blank_allow_if<Bool, ResT>("false", true)), Ok{false});
    EXPECT_EQ(
        (t.validate_allow_blank_allow_if<Bool, ResT>("on", true)),
        Err{"abc: ABC has invalid value"});
    EXPECT_EQ(
        (t.validate_allow_blank_allow_if<Bool, ResT>("", true)),
        Err{"abc: ABC has invalid value"});
    EXPECT_EQ(
        (t.validate_allow_blank_allow_if<Bool, ResT>(nullopt, true)),
        Err{"abc: ABC is not set"});
    EXPECT_EQ(
        (t.validate_allow_blank_allow_if<Bool, ResT>("true", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        (t.validate_allow_blank_allow_if<Bool, ResT>("false", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        (t.validate_allow_blank_allow_if<Bool, ResT>("on", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        (t.validate_allow_blank_allow_if<Bool, ResT>("", false)),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ((t.validate_allow_blank_allow_if<Bool, ResT>(nullopt, false)), Ok{nullopt});
}

// NOLINTNEXTLINE
TEST(form_validation, bool) { test_bool<bool>(); }

// NOLINTNEXTLINE
TEST(form_validation, bool_allow_blank) { test_bool_allow_blank<bool>(); }

// NOLINTNEXTLINE
TEST(form_validation, bool_allow_if) { test_bool_allow_if<bool>(); }

// NOLINTNEXTLINE
TEST(form_validation, bool_allow_blank_allow_if) { test_bool_allow_blank_allow_if<bool>(); }

// NOLINTNEXTLINE
TEST(form_validation, sql_bool) { test_bool<sim::sql_fields::Bool, bool>(); }

// NOLINTNEXTLINE
TEST(form_validation, sql_bool_allow_blank) {
    test_bool_allow_blank<sim::sql_fields::Bool, bool>();
}

// NOLINTNEXTLINE
TEST(form_validation, sql_bool_allow_if) { test_bool_allow_if<sim::sql_fields::Bool, bool>(); }

// NOLINTNEXTLINE
TEST(form_validation, sql_bool_allow_blank_allow_if) {
    test_bool_allow_blank_allow_if<sim::sql_fields::Bool, bool>();
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
static void test_int() {
    ValidationTest t;
    using nl = std::numeric_limits<T>;
    auto err_val_str =
        Err{concat_tostr("abc: ABC is not in range [", nl::min(), ", ", nl::max(), "]")};
    EXPECT_EQ(t.validate<T>("0"), Ok{T{}});
    EXPECT_EQ(t.validate<T>(std::to_string(nl::min())), Ok{nl::min()});
    EXPECT_EQ(t.validate<T>(std::to_string(nl::min() + 1)), Ok{nl::min() + 1});
    if constexpr (std::is_signed_v<T>) {
        EXPECT_EQ(t.validate<T>("-1"), Ok{T{-1}});
        EXPECT_EQ(t.validate<T>("-42"), Ok{T{-42}});
    }
    EXPECT_EQ(t.validate<T>(std::to_string(nl::max())), Ok{nl::max()});
    EXPECT_EQ(t.validate<T>(std::to_string(nl::max() - 1)), Ok{nl::max() - 1});
    EXPECT_EQ(t.validate<T>(""), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate<T>("x"), Err{err_val_str});
    EXPECT_EQ(t.validate<T>("abc"), Err{err_val_str});
    EXPECT_EQ(t.validate<T>("1-1"), Err{err_val_str});
    EXPECT_EQ(t.validate<T>("4.4"), Err{err_val_str});
    EXPECT_EQ(t.validate<T>(nullopt), Err{"abc: ABC is not set"});
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
static void test_int_allow_blank() {
    ValidationTest t;
    using nl = std::numeric_limits<T>;
    auto err_val_str =
        Err{concat_tostr("abc: ABC is not in range [", nl::min(), ", ", nl::max(), "]")};
    EXPECT_EQ(t.validate_allow_blank<T>("0"), Ok{T{}});
    EXPECT_EQ(t.validate_allow_blank<T>(std::to_string(nl::min())), Ok{nl::min()});
    EXPECT_EQ(t.validate_allow_blank<T>(std::to_string(nl::min() + 1)), Ok{nl::min() + 1});
    if constexpr (std::is_signed_v<T>) {
        EXPECT_EQ(t.validate_allow_blank<T>("-1"), Ok{T{-1}});
        EXPECT_EQ(t.validate_allow_blank<T>("-42"), Ok{T{-42}});
    }
    EXPECT_EQ(t.validate_allow_blank<T>(std::to_string(nl::max())), Ok{nl::max()});
    EXPECT_EQ(t.validate_allow_blank<T>(std::to_string(nl::max() - 1)), Ok{nl::max() - 1});
    EXPECT_EQ(t.validate_allow_blank<T>(""), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank<T>("x"), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank<T>("abc"), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank<T>("1-1"), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank<T>("4.4"), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank<T>(nullopt), Err{"abc: ABC is not set"});
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
static void test_int_allow_if() {
    ValidationTest t;
    using nl = std::numeric_limits<T>;
    auto err_val_str =
        Err{concat_tostr("abc: ABC is not in range [", nl::min(), ", ", nl::max(), "]")};
    EXPECT_EQ(t.validate_allow_if<T>("0", true), Ok{T{}});
    EXPECT_EQ(t.validate_allow_if<T>(std::to_string(nl::min()), true), Ok{nl::min()});
    EXPECT_EQ(t.validate_allow_if<T>(std::to_string(nl::min() + 1), true), Ok{nl::min() + 1});
    if constexpr (std::is_signed_v<T>) {
        EXPECT_EQ(t.validate_allow_if<T>("-1", true), Ok{T{-1}});
        EXPECT_EQ(t.validate_allow_if<T>("-42", true), Ok{T{-42}});
    }
    EXPECT_EQ(t.validate_allow_if<T>(std::to_string(nl::max()), true), Ok{nl::max()});
    EXPECT_EQ(t.validate_allow_if<T>(std::to_string(nl::max() - 1), true), Ok{nl::max() - 1});
    EXPECT_EQ(t.validate_allow_if<T>("", true), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate_allow_if<T>("x", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_if<T>("abc", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_if<T>("1-1", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_if<T>("4.4", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_if<T>(nullopt, true), Err{"abc: ABC is not set"});

    for (auto field_val : std::initializer_list<string>{
             "0", std::to_string(nl::min()), std::to_string(nl::min() + 1), "-1", "-42",
             std::to_string(nl::max()), std::to_string(nl::max() - 1), "", "x", "abc", "1-1",
             "4.4"})
    {
        EXPECT_EQ(
            t.validate_allow_if<T>(field_val, false),
            Err{"abc: ABC should not be sent within request at all"});
    }
    EXPECT_EQ(t.validate_allow_if<T>(nullopt, false), Ok{nullopt});
}

template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
static void test_int_allow_blank_allow_if() {
    ValidationTest t;
    using nl = std::numeric_limits<T>;
    auto err_val_str =
        Err{concat_tostr("abc: ABC is not in range [", nl::min(), ", ", nl::max(), "]")};
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("0", true), Ok{T{}});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>(std::to_string(nl::min()), true), Ok{nl::min()});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>(std::to_string(nl::min() + 1), true),
        Ok{nl::min() + 1});
    if constexpr (std::is_signed_v<T>) {
        EXPECT_EQ(t.validate_allow_blank_allow_if<T>("-1", true), Ok{T{-1}});
        EXPECT_EQ(t.validate_allow_blank_allow_if<T>("-42", true), Ok{T{-42}});
    }
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>(std::to_string(nl::max()), true), Ok{nl::max()});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>(std::to_string(nl::max() - 1), true),
        Ok{nl::max() - 1});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("x", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("abc", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("1-1", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("4.4", true), Err{err_val_str});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>(nullopt, true), Err{"abc: ABC is not set"});

    for (auto field_val : std::initializer_list<string>{
             "0", std::to_string(nl::min()), std::to_string(nl::min() + 1), "-1", "-42",
             std::to_string(nl::max()), std::to_string(nl::max() - 1), "", "x", "abc", "1-1",
             "4.4"})
    {
        EXPECT_EQ(
            t.validate_allow_blank_allow_if<T>(field_val, false),
            Err{"abc: ABC should not be sent within request at all"});
    }
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>(nullopt, false), Ok{nullopt});
}

#define test_integers(func)                                    \
    func<int>(); /* NOLINT(bugprone-macro-parentheses) */      \
    func<ssize_t>(); /* NOLINT(bugprone-macro-parentheses) */  \
    func<int8_t>(); /* NOLINT(bugprone-macro-parentheses) */   \
    func<int16_t>(); /* NOLINT(bugprone-macro-parentheses) */  \
    func<int32_t>(); /* NOLINT(bugprone-macro-parentheses) */  \
    func<int64_t>(); /* NOLINT(bugprone-macro-parentheses) */  \
                                                               \
    func<unsigned>(); /* NOLINT(bugprone-macro-parentheses) */ \
    func<size_t>(); /* NOLINT(bugprone-macro-parentheses) */   \
    func<uint8_t>(); /* NOLINT(bugprone-macro-parentheses) */  \
    func<uint16_t>(); /* NOLINT(bugprone-macro-parentheses) */ \
    func<uint32_t>(); /* NOLINT(bugprone-macro-parentheses) */ \
    func<uint64_t>() /* NOLINT(bugprone-macro-parentheses) */

// NOLINTNEXTLINE
TEST(form_validation, integers) { test_integers(test_int); }
// NOLINTNEXTLINE
TEST(form_validation, integers_allow_blank) { test_integers(test_int_allow_blank); }
// NOLINTNEXTLINE
TEST(form_validation, integers_allow_if) { test_integers(test_int_allow_if); }
// NOLINTNEXTLINE
TEST(form_validation, integers_allow_blank_allow_if) {
    test_integers(test_int_allow_blank_allow_if);
}

static constexpr auto predicate = [](const sim::sql_fields::Varbinary<20>& x) {
    return x.size % 10 == 7;
};
static constexpr char predicate_description[] = "does not have length ending with 7";

// NOLINTNEXTLINE
TEST(form_validation, satisfying_predicate) {
    ValidationTest t;
    using sim::sql_fields::SatisfyingPredicate;
    using sim::sql_fields::Varbinary;
    using T = SatisfyingPredicate<Varbinary<20>, predicate, predicate_description>;

    EXPECT_EQ(t.validate<T>("1234567"), Ok{"1234567"});
    EXPECT_EQ(t.validate<T>("12345678901234567"), Ok{"12345678901234567"});
    EXPECT_EQ(t.validate<T>("test"), Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate<T>("123456789012345678"),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate<T>("1234567890123456789"),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate<T>("12345678901234567890"),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate<T>("123456789012345678901"),
        Err{"abc: ABC cannot be longer than 20 bytes"});
    EXPECT_EQ(t.validate<T>(""), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate<T>(nullopt), Err{"abc: ABC is not set"});
}

// NOLINTNEXTLINE
TEST(form_validation, satisfying_predicate_allow_blank) {
    ValidationTest t;
    using sim::sql_fields::SatisfyingPredicate;
    using sim::sql_fields::Varbinary;
    using T = SatisfyingPredicate<Varbinary<20>, predicate, predicate_description>;

    EXPECT_EQ(t.validate_allow_blank<T>("1234567"), Ok{"1234567"});
    EXPECT_EQ(t.validate_allow_blank<T>("12345678901234567"), Ok{"12345678901234567"});
    EXPECT_EQ(
        t.validate_allow_blank<T>("test"), Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank<T>("123456789012345678"),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank<T>("1234567890123456789"),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank<T>("12345678901234567890"),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank<T>("123456789012345678901"),
        Err{"abc: ABC cannot be longer than 20 bytes"});
    EXPECT_EQ(
        t.validate_allow_blank<T>(""), Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(t.validate_allow_blank<T>(nullopt), Err{"abc: ABC is not set"});
}

// NOLINTNEXTLINE
TEST(form_validation, satisfying_predicate_allow_if) {
    ValidationTest t;
    using sim::sql_fields::SatisfyingPredicate;
    using sim::sql_fields::Varbinary;
    using T = SatisfyingPredicate<Varbinary<20>, predicate, predicate_description>;

    EXPECT_EQ(t.validate_allow_if<T>("1234567", true), Ok{"1234567"});
    EXPECT_EQ(t.validate_allow_if<T>("12345678901234567", true), Ok{"12345678901234567"});
    EXPECT_EQ(
        t.validate_allow_if<T>("test", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_if<T>("123456789012345678", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_if<T>("1234567890123456789", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_if<T>("12345678901234567890", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_if<T>("123456789012345678901", true),
        Err{"abc: ABC cannot be longer than 20 bytes"});
    EXPECT_EQ(t.validate_allow_if<T>("", true), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate_allow_if<T>(nullopt, true), Err{"abc: ABC is not set"});

    EXPECT_EQ(
        t.validate_allow_if<T>("1234567", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("12345678901234567", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("test", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("123456789012345678", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("1234567890123456789", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("12345678901234567890", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("123456789012345678901", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(t.validate_allow_if<T>(nullopt, false), Ok{nullopt});
}

// NOLINTNEXTLINE
TEST(form_validation, satisfying_predicate_allow_blank_allow_if) {
    ValidationTest t;
    using sim::sql_fields::SatisfyingPredicate;
    using sim::sql_fields::Varbinary;
    using T = SatisfyingPredicate<Varbinary<20>, predicate, predicate_description>;

    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("1234567", true), Ok{"1234567"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("12345678901234567", true),
        Ok{"12345678901234567"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("test", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("123456789012345678", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("1234567890123456789", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("12345678901234567890", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("123456789012345678901", true),
        Err{"abc: ABC cannot be longer than 20 bytes"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("", true),
        Err{"abc: ABC does not have length ending with 7"});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>(nullopt, true), Err{"abc: ABC is not set"});

    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("1234567", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("12345678901234567", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("test", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("123456789012345678", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("1234567890123456789", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("12345678901234567890", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("123456789012345678901", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>(nullopt, false), Ok{nullopt});
}

ENUM_WITH_STRING_CONVERSIONS(TestEnum, uint16_t,
    (FIRST, 111, "> 1 <")
    (SECOND, 222, "> 2 <")
    (THIRD, 333, "> 3 <")
);

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions) {
    ValidationTest t;
    using T = EnumVal<TestEnum>;
    EXPECT_EQ(t.validate<T>("> 1 <"), Ok{TestEnum::FIRST});
    EXPECT_EQ(t.validate<T>("> 2 <"), Ok{TestEnum::SECOND});
    EXPECT_EQ(t.validate<T>("> 3 <"), Ok{TestEnum::THIRD});
    EXPECT_EQ(t.validate<T>(" > 1 <"), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate<T>("> 1 < "), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate<T>("> 1 <> 1 <"), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate<T>(""), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate<T>(nullopt), Err{"abc: ABC is not set"});
}

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions_allow_blank) {
    ValidationTest t;
    using T = EnumVal<TestEnum>;
    EXPECT_EQ(t.validate_allow_blank<T>("> 1 <"), Ok{TestEnum::FIRST});
    EXPECT_EQ(t.validate_allow_blank<T>("> 2 <"), Ok{TestEnum::SECOND});
    EXPECT_EQ(t.validate_allow_blank<T>("> 3 <"), Ok{TestEnum::THIRD});
    EXPECT_EQ(t.validate_allow_blank<T>(" > 1 <"), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_blank<T>("> 1 < "), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_blank<T>("> 1 <> 1 <"), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_blank<T>(""), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_blank<T>(nullopt), Err{"abc: ABC is not set"});
}

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions_allow_if) {
    ValidationTest t;
    using T = EnumVal<TestEnum>;
    EXPECT_EQ(t.validate_allow_if<T>("> 1 <", true), Ok{TestEnum::FIRST});
    EXPECT_EQ(t.validate_allow_if<T>("> 2 <", true), Ok{TestEnum::SECOND});
    EXPECT_EQ(t.validate_allow_if<T>("> 3 <", true), Ok{TestEnum::THIRD});
    EXPECT_EQ(t.validate_allow_if<T>(" > 1 <", true), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_if<T>("> 1 < ", true), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_if<T>("> 1 <> 1 <", true), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_if<T>("", true), Err{"abc: ABC cannot be blank"});
    EXPECT_EQ(t.validate_allow_if<T>(nullopt, true), Err{"abc: ABC is not set"});

    EXPECT_EQ(
        t.validate_allow_if<T>("> 1 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("> 2 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("> 3 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>(" > 1 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("> 1 < ", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("> 1 <> 1 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_if<T>("", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(t.validate_allow_if<T>(nullopt, false), Ok{nullopt});
}

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions_allow_blank_allow_if) {
    ValidationTest t;
    using T = EnumVal<TestEnum>;
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("> 1 <", true), Ok{TestEnum::FIRST});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("> 2 <", true), Ok{TestEnum::SECOND});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("> 3 <", true), Ok{TestEnum::THIRD});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>(" > 1 <", true), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("> 1 < ", true), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("> 1 <> 1 <", true),
        Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>("", true), Err{"abc: ABC has invalid value"});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>(nullopt, true), Err{"abc: ABC is not set"});

    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("> 1 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("> 2 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("> 3 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>(" > 1 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("> 1 < ", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("> 1 <> 1 <", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(
        t.validate_allow_blank_allow_if<T>("", false),
        Err{"abc: ABC should not be sent within request at all"});
    EXPECT_EQ(t.validate_allow_blank_allow_if<T>(nullopt, false), Ok{nullopt});
}

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions_enum_caps) {
    auto do_validate = [&](optional<string> form_field_value, bool cap1, bool cap2,
                           bool cap3) -> Result<EnumVal<TestEnum>, string> {
        FormFields ff;
        if (form_field_value) {
            ff.add_field("xyz_field", std::move(*form_field_value));
        }
        constexpr ApiParam<EnumVal<TestEnum>> param_abc{"xyz_field", "AXBYCZ"};
        VALIDATE(ff, [&](auto&& errors) { return Err{errors}; },
            (abc, param_abc, ENUM_CAPS(
                (FIRST, cap1)
                (SECOND, cap2)
                (THIRD, cap3)
            ))
        );
        static_assert(std::is_same_v<decltype(abc), EnumVal<TestEnum>>);
        return Ok{abc};
    };
    for (auto [idx, str] : enumerate_view(std::array{"> 1 <", "> 2 <", "> 3 <"})) {
        for (auto cap1 : {true, false}) {
            for (auto cap2 : {true, false}) {
                for (auto cap3 : {true, false}) {
                    std::array caps = {cap1, cap2, cap3};
                    std::array variants = {TestEnum::FIRST, TestEnum::SECOND, TestEnum::THIRD};
                    using RT = Result<TestEnum, const char*>;
                    EXPECT_EQ(
                        do_validate(str, cap1, cap2, cap3),
                        caps[idx]
                            ? RT{Ok{variants[idx]}}
                            : RT{Err{
                                  "xyz_field: AXBYCZ selects option to which you do not have "
                                  "permission"}})
                        << "idx: " << idx << " caps: " << cap1 << ", " << cap2 << ", " << cap3;
                }
            }
        }
    }
    for (auto cap1 : {true, false}) {
        for (auto cap2 : {true, false}) {
            for (auto cap3 : {true, false}) {
                EXPECT_EQ(
                    do_validate(" > 1 <", cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ has invalid value"});
                EXPECT_EQ(
                    do_validate("> 1 < ", cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ has invalid value"});
                EXPECT_EQ(
                    do_validate("> 1 <> 1 <", cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ has invalid value"});
                EXPECT_EQ(
                    do_validate("", cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ cannot be blank"});
                EXPECT_EQ(
                    do_validate(nullopt, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ is not set"});
            }
        }
    }
}

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions_enum_caps_allow_if) {
    auto do_validate = [&](optional<string> form_field_value, bool condition, bool cap1,
                           bool cap2,
                           bool cap3) -> Result<optional<EnumVal<TestEnum>>, string> {
        FormFields ff;
        if (form_field_value) {
            ff.add_field("xyz_field", std::move(*form_field_value));
        }
        constexpr ApiParam<EnumVal<TestEnum>> param_abc{"xyz_field", "AXBYCZ"};
        VALIDATE(ff, [&](auto&& errors) { return Err{errors}; },
            (abc, param_abc, ENUM_CAPS_ALLOW_IF(condition,
                (FIRST, cap1)
                (SECOND, cap2)
                (THIRD, cap3)
            ))
        );
        static_assert(std::is_same_v<decltype(abc), optional<EnumVal<TestEnum>>>);
        return Ok{abc};
    };
    // ALLOW_IF condition == true
    for (auto [idx, str] : enumerate_view(std::array{"> 1 <", "> 2 <", "> 3 <"})) {
        for (auto cap1 : {true, false}) {
            for (auto cap2 : {true, false}) {
                for (auto cap3 : {true, false}) {
                    std::array caps = {cap1, cap2, cap3};
                    std::array variants = {TestEnum::FIRST, TestEnum::SECOND, TestEnum::THIRD};
                    using RT = Result<TestEnum, const char*>;
                    EXPECT_EQ(
                        do_validate(str, true, cap1, cap2, cap3),
                        caps[idx]
                            ? RT{Ok{variants[idx]}}
                            : RT{Err{
                                  "xyz_field: AXBYCZ selects option to which you do not have "
                                  "permission"}})
                        << "idx: " << idx << " caps: " << cap1 << ", " << cap2 << ", " << cap3;
                }
            }
        }
    }
    for (auto cap1 : {true, false}) {
        for (auto cap2 : {true, false}) {
            for (auto cap3 : {true, false}) {
                EXPECT_EQ(
                    do_validate(" > 1 <", true, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ has invalid value"});
                EXPECT_EQ(
                    do_validate("> 1 < ", true, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ has invalid value"});
                EXPECT_EQ(
                    do_validate("> 1 <> 1 <", true, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ has invalid value"});
                EXPECT_EQ(
                    do_validate("", true, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ cannot be blank"});
                EXPECT_EQ(
                    do_validate(nullopt, true, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ is not set"});
            }
        }
    }
    // ALLOW_IF condition == false
    for (auto str : {"> 1 <", "> 2 <", "> 3 <"}) {
        for (auto cap1 : {true, false}) {
            for (auto cap2 : {true, false}) {
                for (auto cap3 : {true, false}) {
                    EXPECT_EQ(
                        do_validate(str, false, cap1, cap2, cap3),
                        Err{"xyz_field: AXBYCZ should not be sent within request at all"})
                        << "str: " << str << " caps: " << cap1 << ", " << cap2 << ", " << cap3;
                }
            }
        }
    }
    for (auto cap1 : {true, false}) {
        for (auto cap2 : {true, false}) {
            for (auto cap3 : {true, false}) {
                EXPECT_EQ(
                    do_validate(" > 1 <", false, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ should not be sent within request at all"});
                EXPECT_EQ(
                    do_validate("> 1 < ", false, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ should not be sent within request at all"});
                EXPECT_EQ(
                    do_validate("> 1 <> 1 <", false, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ should not be sent within request at all"});
                EXPECT_EQ(
                    do_validate("", false, cap1, cap2, cap3),
                    Err{"xyz_field: AXBYCZ should not be sent within request at all"});
                EXPECT_EQ(do_validate(nullopt, false, cap1, cap2, cap3), Ok{nullopt});
            }
        }
    }
}
