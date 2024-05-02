#include "../../../src/web_server/http/form_fields.hh"
#include "../../../src/web_server/http/form_validation.hh"

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <sim/sql/fields/blob.hh>
#include <sim/sql/fields/satisfying_predicate.hh>
#include <sim/sql/fields/varbinary.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/enum_val.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/macros/stringify.hh>
#include <simlib/random_bytes.hh>
#include <simlib/ranges.hh>
#include <simlib/result.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <string>
#include <type_traits>

using std::array;
using std::nullopt;
using std::optional;
using std::string;
using std::string_view;
using web_server::http::ApiParam;
using web_server::http::FormFields;

namespace {

template <class T>
constexpr inline ApiParam<T> api_param = {"abc", "ABC"};

template <class FieldType>
class ValidationTest {
    ApiParam<FieldType> api_param;
    FormFields ff; // Needed here to ensure long-enough lifetime, if validate_*() returns
                   // Ok<CStringView>

    void set_form_fields(optional<string> form_field_value) {
        ff = FormFields{};
        if (form_field_value) {
            ff.add_field(api_param.name.to_string(), std::move(*form_field_value));
        }
    }

    Result<FieldType, string> validate_required(optional<string> form_field_value) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, REQUIRED)
        );
        static_assert(std::is_same_v<decltype(abc), FieldType>);
        return Ok{abc};
    }

    Result<optional<FieldType>, string> validate_optional(optional<string> form_field_value) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, OPTIONAL)
        );
        static_assert(std::is_same_v<decltype(abc), optional<FieldType>>);
        return Ok{abc};
    }

    Result<optional<FieldType>, string>
    validate_allowed_only_if(bool condition, optional<string> form_field_value) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, ALLOWED_ONLY_IF(condition))
        );
        static_assert(std::is_same_v<decltype(abc), optional<FieldType>>);
        return Ok{abc};
    }

    Result<optional<FieldType>, string>
    validate_required_and_allowed_only_if(bool condition, optional<string> form_field_value) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, REQUIRED_AND_ALLOWED_ONLY_IF(condition))
        );
        static_assert(std::is_same_v<decltype(abc), optional<FieldType>>);
        return Ok{abc};
    }

public:
    explicit ValidationTest(ApiParam<FieldType> api_param) : api_param{std::move(api_param)} {
        SCOPED_TRACE(__PRETTY_FUNCTION__);
        EXPECT_EQ(validate_required(nullopt), Err{"abc: ABC is not set"});
        EXPECT_EQ(validate_optional(nullopt), Ok{nullopt});
        EXPECT_EQ(validate_allowed_only_if(true, nullopt), Ok{nullopt});
        EXPECT_EQ(validate_allowed_only_if(false, nullopt), Ok{nullopt});
        EXPECT_EQ(validate_required_and_allowed_only_if(true, nullopt), Err{"abc: ABC is not set"});
        EXPECT_EQ(validate_required_and_allowed_only_if(false, nullopt), Ok{nullopt});
    }

    template <class T>
    void check(string form_field_value, const T& expected_validation_result) {
        SCOPED_TRACE(__PRETTY_FUNCTION__);
        EXPECT_EQ(validate_required(form_field_value), expected_validation_result);
        EXPECT_EQ(validate_optional(form_field_value), expected_validation_result);
        EXPECT_EQ(validate_allowed_only_if(true, form_field_value), expected_validation_result);
        EXPECT_EQ(
            validate_allowed_only_if(false, form_field_value),
            Err{"abc: ABC should not be sent in the request at all"}
        );
        EXPECT_EQ(
            validate_required_and_allowed_only_if(true, form_field_value),
            expected_validation_result
        );
        EXPECT_EQ(
            validate_required_and_allowed_only_if(false, form_field_value),
            Err{"abc: ABC should not be sent in the request at all"}
        );
    }
};

} // namespace

#define VALIDATE_CHECK(validation_test, ...)                                   \
    {                                                                          \
        SCOPED_TRACE(STRINGIFY(VALIDATE_CHECK(validation_test, __VA_ARGS__))); \
        (validation_test).check(__VA_ARGS__);                                  \
    }

template <class Bool>
static void test_bool() {
    ValidationTest t{api_param<Bool>};
    VALIDATE_CHECK(t, "true", Ok{true});
    VALIDATE_CHECK(t, "false", Ok{false});
    VALIDATE_CHECK(t, "on", Err{"abc: ABC has invalid value"});
    VALIDATE_CHECK(t, "", Err{"abc: ABC has invalid value"});
    VALIDATE_CHECK(t, " true", Err{"abc: ABC has invalid value"});
    VALIDATE_CHECK(t, "true ", Err{"abc: ABC has invalid value"});
}

// NOLINTNEXTLINE
TEST(form_validation, bool) { test_bool<bool>(); }

template <class Int>
static void test_int() {
    ValidationTest t{api_param<Int>};
    using nl = std::numeric_limits<Int>;
    VALIDATE_CHECK(t, "0", Ok{Int{}});
    VALIDATE_CHECK(t, concat_tostr(nl::min()), Ok{nl::min()});
    VALIDATE_CHECK(t, concat_tostr(nl::min() + 1), Ok{nl::min() + 1});
    if constexpr (std::is_signed_v<Int>) {
        VALIDATE_CHECK(t, "-1", Ok{Int{-1}});
        VALIDATE_CHECK(t, "-42", Ok{Int{-42}});
    }
    VALIDATE_CHECK(t, concat_tostr(nl::max()), Ok{nl::max()});
    VALIDATE_CHECK(t, concat_tostr(nl::max() - 1), Ok{nl::max() - 1});
    auto err_val_str =
        Err{concat_tostr("abc: ABC is not an integer from range [", nl::min(), ", ", nl::max(), "]")
        };
    VALIDATE_CHECK(t, "", Err{err_val_str});
    VALIDATE_CHECK(t, "x", Err{err_val_str});
    VALIDATE_CHECK(t, "abc", Err{err_val_str});
    VALIDATE_CHECK(t, "1-1", Err{err_val_str});
    VALIDATE_CHECK(t, "4.4", Err{err_val_str});
    VALIDATE_CHECK(t, " 0", Err{err_val_str});
    VALIDATE_CHECK(t, "0 ", Err{err_val_str});
    VALIDATE_CHECK(t, " 123", Err{err_val_str});
    VALIDATE_CHECK(t, "123 ", Err{err_val_str});
    VALIDATE_CHECK(t, " -1", Err{err_val_str});
    VALIDATE_CHECK(t, "-1 ", Err{err_val_str});
}

// NOLINTNEXTLINE
TEST(form_validation, integers) {
    test_int<int>();
    test_int<ssize_t>();
    test_int<int8_t>();
    test_int<int16_t>();
    test_int<int32_t>();
    test_int<int64_t>();

    test_int<unsigned>();
    test_int<size_t>();
    test_int<uint8_t>();
    test_int<uint16_t>();
    test_int<uint32_t>();
    test_int<uint64_t>();
}

template <class StrType>
static void
test_str_common(ValidationTest<StrType>& t, std::optional<size_t> max_len = std::nullopt) {
    const CStringView test_value = "test value";
    if (max_len >= test_value.size()) {
        VALIDATE_CHECK(t, test_value.to_string(), Ok{test_value});
    }
    for (size_t len = 1; len <= std::min(size_t{100}, max_len.value_or(100)); ++len) {
        auto str = random_bytes(len);
        VALIDATE_CHECK(t, str, Ok{str});
    }
    if (const auto len = max_len.value_or(70000); len > 0) {
        auto str = random_bytes(len);
        VALIDATE_CHECK(t, str, Ok{str});
    }
    if (max_len) {
        for (size_t len = *max_len + 1; len < *max_len + 16; ++len) {
            auto str = random_bytes(len);
            VALIDATE_CHECK(
                t, str, Err{concat_tostr("abc: ABC cannot be longer than ", *max_len, " bytes")}
            );
        }
    }
}

template <class StrType>
static void test_str(std::optional<size_t> max_len = std::nullopt) {
    {
        ValidationTest t{api_param<StrType>};
        test_str_common(t, max_len);
        VALIDATE_CHECK(t, "", Err{"abc: ABC cannot be blank"})
    }
    {
        ValidationTest t{allow_blank(api_param<StrType>)};
        test_str_common(t, max_len);
        VALIDATE_CHECK(t, "", Ok{""})
    }
    {
        ValidationTest t{allow_blank_if(api_param<StrType>, false)};
        test_str_common(t, max_len);
        VALIDATE_CHECK(t, "", Err{"abc: ABC cannot be blank"})
    }
    {
        ValidationTest t{allow_blank_if(api_param<StrType>, true)};
        test_str_common(t, max_len);
        VALIDATE_CHECK(t, "", Ok{""})
    }
}

// NOLINTNEXTLINE
TEST(form_validation, cstring_view) { test_str<CStringView>(); }

// NOLINTNEXTLINE
TEST(form_validation, sql_blob) { test_str<sim::sql::fields::Blob>(); }

template <size_t max_len>
static void varbinary_test() {
    test_str<sim::sql::fields::Varbinary<max_len>>(max_len);
}

// NOLINTNEXTLINE
TEST(form_validation, sql_varbinary) {
    varbinary_test<0>();
    varbinary_test<1>();
    varbinary_test<2>();
    varbinary_test<3>();
    varbinary_test<4>();
    varbinary_test<5>();
    varbinary_test<6>();
    varbinary_test<7>();
    varbinary_test<8>();
    varbinary_test<13>();
    varbinary_test<14>();
    varbinary_test<15>();
    varbinary_test<16>();
    varbinary_test<32>();
    varbinary_test<42>();
    varbinary_test<57>();
}

template <class T>
static constexpr bool always_true(const T& /*unused*/) noexcept {
    return true;
}

template <class T>
static constexpr bool ends_with_xd(const T& str) {
    return has_suffix(str, "xd");
}

// NOLINTNEXTLINE
TEST(form_validation, satisfying_predicate_string) {
    using sim::sql::fields::SatisfyingPredicate;
    static constexpr char predicate_description[] = "a string ending with \"xd\"";
    constexpr size_t varbinary_max_len = 14;
    test_str<sim::sql::fields::SatisfyingPredicate<
        sim::sql::fields::Varbinary<varbinary_max_len>,
        always_true,
        predicate_description>>(varbinary_max_len);
    test_str<
        sim::sql::fields::SatisfyingPredicate<CStringView, always_true, predicate_description>>();
    test_str<sim::sql::fields::
                 SatisfyingPredicate<sim::sql::fields::Blob, always_true, predicate_description>>();

    auto api_param_val = api_param<SatisfyingPredicate<
        sim::sql::fields::Varbinary<varbinary_max_len>,
        ends_with_xd,
        predicate_description>>;
    for (auto api_param :
         {api_param_val,
          allow_blank(api_param_val),
          allow_blank_if(api_param_val, false),
          allow_blank_if(api_param_val, true)})
    {
        ValidationTest t{api_param};
        VALIDATE_CHECK(t, "xd", Ok{"xd"});
        VALIDATE_CHECK(t, "aoenhusxd", Ok{"aoenhusxd"});
        VALIDATE_CHECK(t, "xxd", Ok{"xxd"});
        VALIDATE_CHECK(t, "dx", Err{"abc: ABC has to be a string ending with \"xd\""});
        VALIDATE_CHECK(t, "xdxdxdxdx", Err{"abc: ABC has to be a string ending with \"xd\""});
        VALIDATE_CHECK(t, "123456789012xd", Ok{"123456789012xd"});
        VALIDATE_CHECK(t, "1234567890123xd", Err{"abc: ABC cannot be longer than 14 bytes"});
        VALIDATE_CHECK(t, "123456789012345", Err{"abc: ABC cannot be longer than 14 bytes"});
    }
}

ENUM_WITH_STRING_CONVERSIONS(TestEnum, uint16_t,
    (FIRST, 111, "> 1 <")
    (SECOND, 222, "> 2 <")
    (THIRD, 333, "> 3 <")
);

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions) {
    ValidationTest t{api_param<TestEnum>};
    VALIDATE_CHECK(t, "> 1 <", Ok{TestEnum::FIRST});
    VALIDATE_CHECK(t, "> 2 <", Ok{TestEnum::SECOND});
    VALIDATE_CHECK(t, "> 3 <", Ok{TestEnum::THIRD});
    VALIDATE_CHECK(t, " > 1 <", Err{"abc: ABC has invalid value"});
    VALIDATE_CHECK(t, "> 1 < ", Err{"abc: ABC has invalid value"});
    VALIDATE_CHECK(t, "> 1 <> 1 <", Err{"abc: ABC has invalid value"});
    VALIDATE_CHECK(t, "", Err{"abc: ABC has invalid value"});
}

class EnumCapsValidationTest {
    using FieldType = TestEnum;
    ApiParam<FieldType> api_param = ::api_param<FieldType>;
    FormFields ff; // Needed here to ensure long-enough lifetime, if validate_*() returns
                   // Ok<CStringView>

    void set_form_fields(optional<string> form_field_value) {
        ff = FormFields{};
        if (form_field_value) {
            ff.add_field(api_param.name.to_string(), std::move(*form_field_value));
        }
    }

    Result<FieldType, string>
    validate_required(array<bool, 3> caps, optional<string> form_field_value) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, REQUIRED_ENUM_CAPS(
                (FIRST, caps[0])
                (SECOND, caps[1])
                (THIRD, caps[2])
            ))
        );
        static_assert(std::is_same_v<decltype(abc), FieldType>);
        return Ok{abc};
    }

    Result<optional<FieldType>, string>
    validate_optional(array<bool, 3> caps, optional<string> form_field_value) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, OPTIONAL_ENUM_CAPS(
                (FIRST, caps[0])
                (SECOND, caps[1])
                (THIRD, caps[2])
            ))
        );
        static_assert(std::is_same_v<decltype(abc), optional<FieldType>>);
        return Ok{abc};
    }

    Result<optional<FieldType>, string> validate_allowed_only_if(
        bool condition, array<bool, 3> caps, optional<string> form_field_value
    ) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, ALLOWED_ONLY_IF_ENUM_CAPS(condition,
                (FIRST, caps[0])
                (SECOND, caps[1])
                (THIRD, caps[2])
            ))
        );
        static_assert(std::is_same_v<decltype(abc), optional<FieldType>>);
        return Ok{abc};
    }

    Result<optional<FieldType>, string> validate_required_and_allowed_only_if(
        bool condition, array<bool, 3> caps, optional<string> form_field_value
    ) {
        set_form_fields(std::move(form_field_value));
        VALIDATE(ff, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param, REQUIRED_AND_ALLOWED_ONLY_IF_ENUM_CAPS(condition,
                (FIRST, caps[0])
                (SECOND, caps[1])
                (THIRD, caps[2])
            ))
        );
        static_assert(std::is_same_v<decltype(abc), optional<FieldType>>);
        return Ok{abc};
    }

public:
    EnumCapsValidationTest() {
        SCOPED_TRACE(__PRETTY_FUNCTION__);
        for (auto cap0 : {true, false}) {
            for (auto cap1 : {true, false}) {
                for (auto cap2 : {true, false}) {
                    std::array caps = {cap0, cap1, cap2};
                    EXPECT_EQ(validate_required(caps, nullopt), Err{"abc: ABC is not set"});
                    EXPECT_EQ(validate_optional(caps, nullopt), Ok{nullopt});
                    EXPECT_EQ(validate_allowed_only_if(true, caps, nullopt), Ok{nullopt});
                    EXPECT_EQ(validate_allowed_only_if(false, caps, nullopt), Ok{nullopt});
                    EXPECT_EQ(
                        validate_required_and_allowed_only_if(true, caps, nullopt),
                        Err{"abc: ABC is not set"}
                    );
                    EXPECT_EQ(
                        validate_required_and_allowed_only_if(false, caps, nullopt), Ok{nullopt}
                    );
                }
            }
        }
    }

    template <class T>
    void check(array<bool, 3> caps, string form_field_value, const T& expected_validation_result) {
        SCOPED_TRACE(__PRETTY_FUNCTION__);
        EXPECT_EQ(validate_required(caps, form_field_value), expected_validation_result);
        EXPECT_EQ(validate_optional(caps, form_field_value), expected_validation_result);
        EXPECT_EQ(
            validate_allowed_only_if(true, caps, form_field_value), expected_validation_result
        );
        EXPECT_EQ(
            validate_allowed_only_if(false, caps, form_field_value),
            Err{"abc: ABC should not be sent in the request at all"}
        );
        EXPECT_EQ(
            validate_required_and_allowed_only_if(true, caps, form_field_value),
            expected_validation_result
        );
        EXPECT_EQ(
            validate_required_and_allowed_only_if(false, caps, form_field_value),
            Err{"abc: ABC should not be sent in the request at all"}
        );
    }
};

// NOLINTNEXTLINE
TEST(form_validation, enum_val_with_string_conversions_enum_caps) {
    EnumCapsValidationTest t;
    for (auto cap0 : {true, false}) {
        for (auto cap1 : {true, false}) {
            for (auto cap2 : {true, false}) {
                std::array caps = {cap0, cap1, cap2};
                VALIDATE_CHECK(t, caps, " > 1 <", Err{"abc: ABC has invalid value"});
                VALIDATE_CHECK(t, caps, "> 1 < ", Err{"abc: ABC has invalid value"});
                VALIDATE_CHECK(t, caps, "> 1 <> 1 <", Err{"abc: ABC has invalid value"});
                VALIDATE_CHECK(t, caps, "", Err{"abc: ABC has invalid value"});

                for (auto [idx, str] : enumerate_view(std::array{"> 1 <", "> 2 <", "> 3 <"})) {
                    constexpr std::array variants = {
                        TestEnum::FIRST, TestEnum::SECOND, TestEnum::THIRD
                    };
                    using RT = Result<TestEnum, const char*>;
                    SCOPED_TRACE(
                        concat_tostr("idx: ", idx, " caps: {", cap0, ", ", cap1, ", ", cap2, "}")
                    );
                    VALIDATE_CHECK(
                        t,
                        caps,
                        str,
                        caps[idx] ? RT{Ok{variants[idx]}}
                                  : RT{Err{"abc: ABC selects option to which you do not "
                                           "have "
                                           "permission"}}
                    );
                }
            }
        }
    }
}

// NOLINTNEXTLINE
TEST(form_validation, validate_file_required) {
    auto do_validate = [](FormFields& form_fields
                       ) -> Result<web_server::http::SubmittedFile, string> {
        VALIDATE(form_fields, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param<web_server::http::SubmittedFile>, FILE_REQUIRED)
        );
        static_assert(std::is_same_v<decltype(abc), web_server::http::SubmittedFile>);
        return Ok{abc};
    };
    {
        auto form_fields = FormFields{};
        EXPECT_EQ(do_validate(form_fields), Err{"abc: ABC file is missing"});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "some filename");
        EXPECT_EQ(do_validate(form_fields), Err{"abc: ABC should be a file"});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "");
        EXPECT_EQ(do_validate(form_fields), Err{"abc: ABC should be a file"});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "", "/some/path");
        EXPECT_EQ(do_validate(form_fields), Err{"abc: ABC file is missing (empty filename)"});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "some filename", "/some/path");
        auto res = do_validate(form_fields);
        ASSERT_TRUE(res.is_ok());
        auto submitted_file = std::move(res).unwrap();
        EXPECT_EQ(submitted_file.filename, "some filename");
        EXPECT_EQ(submitted_file.path, "/some/path");
    }
}

// NOLINTNEXTLINE
TEST(form_validation, validate_file_optional) {
    auto do_validate = [](FormFields& form_fields
                       ) -> Result<optional<web_server::http::SubmittedFile>, string> {
        VALIDATE(form_fields, [&](auto&& errors) { return Err{std::forward<decltype(errors)>(errors)}; },
            (abc, api_param<web_server::http::SubmittedFile>, FILE_OPTIONAL)
        );
        static_assert(std::
                          is_same_v<decltype(abc), std::optional<web_server::http::SubmittedFile>>);
        return Ok{abc};
    };
    {
        auto form_fields = FormFields{};
        EXPECT_EQ(do_validate(form_fields), Ok{nullopt});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "some filename");
        EXPECT_EQ(do_validate(form_fields), Err{"abc: ABC should be a file"});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "");
        EXPECT_EQ(do_validate(form_fields), Err{"abc: ABC should be a file"});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "", "/some/path");
        EXPECT_EQ(do_validate(form_fields), Err{"abc: ABC file is missing (empty filename)"});
    }
    {
        auto form_fields = FormFields{};
        form_fields.add_field("abc", "some filename", "/some/path");
        auto res = do_validate(form_fields);
        ASSERT_TRUE(res.is_ok());
        auto submitted_file = std::move(res).unwrap();
        ASSERT_TRUE(submitted_file.has_value());
        EXPECT_EQ(submitted_file->filename, "some filename");
        EXPECT_EQ(submitted_file->path, "/some/path");
    }
}
