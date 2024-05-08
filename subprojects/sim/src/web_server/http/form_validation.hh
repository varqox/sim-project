#pragma once

#include "form_fields.hh"

#include <cstddef>
#include <limits>
#include <optional>
#include <sim/sql/fields/blob.hh>
#include <sim/sql/fields/satisfying_predicate.hh>
#include <sim/sql/fields/varbinary.hh>
#include <simlib/always_false.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/enum_val.hh>
#include <simlib/macros/enum_with_string_conversions.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace web_server::http {

namespace detail {

template <class...>
constexpr inline bool is_str_type = false;

template <>
constexpr inline bool is_str_type<CStringView> = true;

template <size_t MAX_LEN>
constexpr inline bool is_str_type<sim::sql::fields::Varbinary<MAX_LEN>> = true;

template <>
constexpr inline bool is_str_type<sim::sql::fields::Blob> = true;

template <class SqlField, bool (*predicate_func)(const SqlField&), const char* description_str>
constexpr inline bool
    is_str_type<sim::sql::fields::SatisfyingPredicate<SqlField, predicate_func, description_str>> =
        is_str_type<SqlField>;

} // namespace detail

template <class T>
class ApiParam {
public:
    CStringView name;
    CStringView description;
    using type = T;

private:
    bool allow_blank_str_val = false;

    constexpr ApiParam(
        std::in_place_t /*unused*/,
        CStringView name,
        CStringView description,
        bool allow_blank_str_val
    ) noexcept
    : name{name}
    , description{description}
    , allow_blank_str_val{allow_blank_str_val} {}

public:
    constexpr ApiParam(CStringView name, CStringView description) noexcept
    : name{name}
    , description{description} {}

    template <
        class OtherT,
        std::enable_if_t<std::is_base_of_v<T, OtherT> && !std::is_same_v<T, OtherT>, int> = 0>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr ApiParam(ApiParam<OtherT> other) noexcept
    : ApiParam{std::in_place, other.name, other.description, other.is_blank_str_val_allowed()} {}

    template <class C>
    constexpr ApiParam(T C::* /*unused*/, CStringView name, CStringView description) noexcept
    : ApiParam{name, description} {}

    [[nodiscard]] constexpr bool is_blank_str_val_allowed() const noexcept {
        return allow_blank_str_val;
    }

    friend ApiParam allow_blank_if(ApiParam api_param, bool cond) {
        static_assert(
            detail::is_str_type<T>, "Allowing blank only makes sense on API params that are strings"
        );
        api_param.allow_blank_str_val = cond;
        return api_param;
    }

    friend ApiParam allow_blank(ApiParam api_param) {
        return allow_blank_if(std::move(api_param), true);
    }
};

namespace detail {

template <class T, class... Arg>
std::nullopt_t append_error(std::string& errors_str, ApiParam<T> api_param, Arg&&... args) {
    if (!errors_str.empty()) {
        errors_str += '\n';
    }
    back_insert(
        errors_str,
        api_param.name,
        ": ",
        api_param.description,
        ' ',
        std::forward<decltype(args)>(args)...
    );
    return std::nullopt;
}

inline std::optional<CStringView>
validate(const ApiParam<CStringView>& api_param, CStringView str_val, std::string& errors_str) {
    if (str_val.empty() and !api_param.is_blank_str_val_allowed()) {
        return append_error(errors_str, api_param, "cannot be blank");
    }
    return str_val;
}

inline std::optional<sim::sql::fields::Blob> validate(
    const ApiParam<sim::sql::fields::Blob>& api_param, CStringView str_val, std::string& errors_str
) {
    if (str_val.empty() and !api_param.is_blank_str_val_allowed()) {
        return append_error(errors_str, api_param, "cannot be blank");
    }
    using T = typename std::remove_reference_t<decltype(api_param)>::type;
    return T{str_val.to_string()};
}

template <size_t MAX_LEN>
std::optional<sim::sql::fields::Varbinary<MAX_LEN>> validate(
    const ApiParam<sim::sql::fields::Varbinary<MAX_LEN>>& api_param,
    CStringView str_val,
    std::string& errors_str
) {
    if (str_val.empty() and !api_param.is_blank_str_val_allowed()) {
        return append_error(errors_str, api_param, "cannot be blank");
    }
    using T = typename std::remove_reference_t<decltype(api_param)>::type;
    if (str_val.size() > T::max_len) {
        return append_error(errors_str, api_param, "cannot be longer than ", T::max_len, " bytes");
    }
    return T{str_val.to_string()};
}

template <class T>
std::enable_if_t<is_enum_with_string_conversions<T>, std::optional<T>>
validate(const ApiParam<T>& api_param, CStringView str_val, std::string& errors_str) {
    auto opt = T::from_str(str_val);
    if (!opt) {
        return append_error(errors_str, api_param, "has invalid value");
    }
    return std::move(*opt);
}

inline std::optional<bool>
validate(const ApiParam<bool>& api_param, CStringView str_val, std::string& errors_str) {
    if (str_val == "true") {
        return true;
    }
    if (str_val == "false") {
        return false;
    }
    return append_error(errors_str, api_param, "has invalid value");
}

template <class T>
std::enable_if_t<std::is_integral_v<T> and !std::is_same_v<T, bool>, std::optional<T>>
validate(const ApiParam<T>& api_param, CStringView str_val, std::string& errors_str) {
    if (auto opt = str2num<T>(str_val); opt) {
        return std::move(*opt);
    }
    static constexpr auto min_val = std::numeric_limits<T>::min();
    static constexpr auto max_val = std::numeric_limits<T>::max();
    return append_error(
        errors_str, api_param, "is not an integer from range [", min_val, ", ", max_val, ']'
    );
}

template <class SqlField, bool (*predicate_func)(const SqlField&), const char* description_str>
std::optional<sim::sql::fields::SatisfyingPredicate<SqlField, predicate_func, description_str>>
validate(
    const ApiParam<
        sim::sql::fields::SatisfyingPredicate<SqlField, predicate_func, description_str>>&
        api_param,
    CStringView str_val,
    std::string& errors_str
) {
    std::optional<SqlField> opt = validate(ApiParam<SqlField>{api_param}, str_val, errors_str);
    if (!opt) {
        return std::nullopt;
    }
    using T = typename std::remove_const_t<std::remove_reference_t<decltype(api_param)>>::type;
    if (not T::predicate(*opt)) {
        return append_error(errors_str, api_param, "has to be ", T::description);
    }
    return std::move(*opt);
}

} // namespace detail

// Returns:
// - field set and valid -> Some(parsed_value)
// - field not set or invalid -> None
template <class T>
auto validate_required(
    ApiParam<T> api_param, const FormFields& form_fields, std::string& errors_str
) {
    const auto str_val_opt = form_fields.get(api_param.name);
    return str_val_opt ? detail::validate(api_param, *str_val_opt, errors_str)
                       : detail::append_error(errors_str, api_param, "is not set");
}

// Returns:
// - field set and valid -> Some(Some(parsed_value))
// - field set and invalid -> None
// - field not set -> Some(None)
template <class T>
auto validate_optional(
    ApiParam<T> api_param, const FormFields& form_fields, std::string& errors_str
) {
    const auto str_val_opt = form_fields.get(api_param.name);
    auto do_validate = [&] { return detail::validate(api_param, *str_val_opt, errors_str); };
    using ResT = std::optional<decltype(do_validate())>;
    if (!str_val_opt) {
        return ResT{std::in_place, std::nullopt};
    }
    auto opt = do_validate();
    if (!opt) {
        return ResT{std::nullopt}; // validation failure (error is already set)
    }
    return ResT{std::move(opt)};
}

// Returns:
// - condition and field set and valid -> Some(Some(parsed_value))
// - condition and field set and invalid -> None
// - condition and field not set -> Some(None)
// - !condition and field set -> None
// - !condition and field not set -> Some(None)
template <class T>
auto validate_allowed_only_if(
    ApiParam<T> api_param, const FormFields& form_fields, std::string& errors_str, bool condition
) {
    const auto str_val_opt = form_fields.get(api_param.name);
    auto do_validate = [&] { return detail::validate(api_param, *str_val_opt, errors_str); };
    using ResT = std::optional<decltype(do_validate())>;
    if (!str_val_opt) {
        return ResT{std::in_place, std::nullopt};
    }
    if (!condition) {
        return ResT{
            detail::append_error(errors_str, api_param, "should not be sent in the request at all")
        };
    }
    auto opt = do_validate();
    if (!opt) {
        return ResT{std::nullopt}; // validation failure (error is already set)
    }
    return ResT{std::move(opt)};
}

// Returns:
// - condition and field set and valid -> Some(Some(parsed_value))
// - condition and field set and invalid -> None
// - condition and field not set -> None
// - !condition and field set -> None
// - !condition and field not set -> Some(None)
template <class T>
auto validate_required_and_allowed_only_if(
    ApiParam<T> api_param, const FormFields& form_fields, std::string& errors_str, bool condition
) {
    const auto str_val_opt = form_fields.get(api_param.name);
    auto do_validate = [&] { return detail::validate(api_param, *str_val_opt, errors_str); };
    using ResT = std::optional<decltype(do_validate())>;
    if (!str_val_opt) {
        if (condition) {
            return ResT{detail::append_error(errors_str, api_param, "is not set")};
        }
        return ResT{std::in_place, std::nullopt};
    }
    if (!condition) {
        return ResT{
            detail::append_error(errors_str, api_param, "should not be sent in the request at all")
        };
    }
    auto opt = do_validate();
    if (!opt) {
        return ResT{std::nullopt}; // validation failure (error is already set)
    }
    return ResT{std::move(opt)};
}

struct SubmittedFile {
    CStringView filename;
    CStringView path;
};

// Returns:
// - file path and filename set and valid -> Some(SubmittedFile)
// - file path not set or invalid -> None
inline std::optional<SubmittedFile> validate_file_required(
    ApiParam<SubmittedFile> api_param, const FormFields& form_fields, std::string& errors_str
) {
    const auto filename_val_opt = form_fields.get(api_param.name);
    const auto file_path_opt = form_fields.file_path(api_param.name);
    if (!filename_val_opt) {
        return detail::append_error(errors_str, api_param, "file is missing");
    }
    if (!file_path_opt) {
        return detail::append_error(errors_str, api_param, "should be a file");
    }
    if (filename_val_opt->empty()) {
        return detail::append_error(errors_str, api_param, "file is missing (empty filename)");
    }
    return SubmittedFile{
        .filename = *filename_val_opt,
        .path = *file_path_opt,
    };
}

// Returns:
// - file path and filename set and valid -> Some(Some(SubmittedFile))
// - file not set -> Some(None)
// - otherwise -> None
inline std::optional<std::optional<SubmittedFile>> validate_file_optional(
    ApiParam<SubmittedFile> api_param, const FormFields& form_fields, std::string& errors_str
) {
    const auto filename_val_opt = form_fields.get(api_param.name);
    const auto file_path_opt = form_fields.file_path(api_param.name);
    if (!filename_val_opt) {
        return std::optional<std::optional<SubmittedFile>>{std::in_place, std::nullopt};
    }
    if (!file_path_opt) {
        return detail::append_error(errors_str, api_param, "should be a file");
    }
    if (filename_val_opt->empty()) {
        return detail::append_error(errors_str, api_param, "file is missing (empty filename)");
    }
    return std::optional{SubmittedFile{
        .filename = *filename_val_opt,
        .path = *file_path_opt,
    }};
}

#define VALIDATE(form_fields, errors_str_to_return_value_func, seq) \
    IMPL_VALIDATE(                                                  \
        form_fields,                                                \
        errors_str_to_return_value_func,                            \
        CAT(_validate_variant_macro_local_variable_, __COUNTER__),  \
        MAP(IMPL_VALIDATE_VAR_NAMES, seq),                          \
        seq                                                         \
    )
#define IMPL_VALIDATE_VAR_NAMES(var_name, ...) (var_name)
#define IMPL_VALIDATE(...) IMPL_VALIDATE2(__VA_ARGS__)
#define IMPL_VALIDATE2(                                                                         \
    form_fields, errors_str_to_return_value_func, validate_variant_var, var_names_seq, seq      \
)                                                                                               \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                            \
    auto validate_variant_var =                                                                 \
        [&] { /* NOLINT(bugprone-macro-parentheses) */                                          \
              std::string errors_str;                                                           \
              IMPL_VALIDATE_DECLARE_VARS(seq, form_fields, errors_str)                          \
              using TupleType = decltype(std::tuple{                                            \
                  MAP_DELIM_FUNC(IMPL_VALIDATE_VAR_NAME_TO_STAR_VAR_NAME, COMMA, var_names_seq) \
              });                                                                               \
              if (MAP_DELIM(IMPL_VALIDATE_VAR_NAME_TO_NOT_VAR_NAME, ||, var_names_seq)) {       \
                  return std::variant<TupleType, std::string>{                                  \
                      std::in_place_type<std::string>, std::move(errors_str)                    \
                  };                                                                            \
              }                                                                                 \
              return std::variant<TupleType, std::string>{                                      \
                  std::in_place_type<TupleType>,                                                \
                  MAP_DELIM_FUNC(IMPL_VALIDATE_VAR_NAME_TO_STAR_VAR_NAME, COMMA, var_names_seq) \
              };                                                                                \
        }();                                                                                    \
    if (auto* validation_errors_str = std::get_if<1>(&(validate_variant_var)))                  \
    { /* NOLINT(bugprone-macro-parentheses) */                                                  \
        return errors_str_to_return_value_func(*validation_errors_str);                         \
    }                                                                                           \
    auto& [MAP_DELIM_FUNC(IMPL_VALIDATE_VAR_NAME, COMMA, var_names_seq)] =                      \
        std::get<0>(validate_variant_var)
#define IMPL_VALIDATE_VAR_NAME(var_name) var_name
#define IMPL_VALIDATE_VAR_NAME_TO_STAR_VAR_NAME(var_name) *var_name
#define IMPL_VALIDATE_VAR_NAME_TO_NOT_VAR_NAME(var_name) !var_name
#define IMPL_VALIDATE_DECLARE_VARS(seq, form_fields, errors_str)         \
    IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES(                               \
        FOLDR(IMPL_VALIDATE_DECLARE_VAR, seq, form_fields, errors_str, ) \
    )
#define IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES(...) \
    IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES2(__VA_ARGS__)
#define IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES2(form_fields, errors_str, ...) __VA_ARGS__
#define IMPL_VALIDATE_DECLARE_VAR(args, form_fields, errors_str, ...)                              \
    form_fields, errors_str,                                                                       \
        IMPL_VALIDATE_DECLARE_VAR2(form_fields, errors_str, IMPL_VALIDATE_DECLARE_VAR_NOOP args, ) \
            __VA_ARGS__
#define IMPL_VALIDATE_DECLARE_VAR_NOOP(...) __VA_ARGS__
#define IMPL_VALIDATE_DECLARE_VAR2(...) IMPL_VALIDATE_DECLARE_VAR3(__VA_ARGS__)
#define IMPL_VALIDATE_DECLARE_VAR3(form_fields, errors_str, var_name, api_param, kind, ...) \
    IMPL_VALIDATE_DECLARE_VAR4(                                                             \
        form_fields,                                                                        \
        errors_str,                                                                         \
        var_name,                                                                           \
        api_param,                                                                          \
        PRIMITIVE_DOUBLE_CAT(IMPL_VALIDATE_DECLARE_VAR_KIND, _, kind)                       \
    )
#define IMPL_VALIDATE_DECLARE_VAR4(...) IMPL_VALIDATE_DECLARE_VAR5(__VA_ARGS__)

#define IMPL_VALIDATE_DECLARE_VAR_KIND_REQUIRED IMPL_VALIDATE_INITIALIZE_VAR, validate_required
#define IMPL_VALIDATE_DECLARE_VAR_KIND_OPTIONAL IMPL_VALIDATE_INITIALIZE_VAR, validate_optional
#define IMPL_VALIDATE_DECLARE_VAR_KIND_ALLOWED_ONLY_IF(condition) \
    IMPL_VALIDATE_INITIALIZE_VAR_WITH_COND, validate_allowed_only_if, condition
#define IMPL_VALIDATE_DECLARE_VAR_KIND_REQUIRED_AND_ALLOWED_ONLY_IF(condition) \
    IMPL_VALIDATE_INITIALIZE_VAR_WITH_COND, validate_required_and_allowed_only_if, condition
#define IMPL_VALIDATE_DECLARE_VAR_KIND_REQUIRED_ENUM_CAPS(caps_seq)                 \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS, caps_seq, IMPL_VALIDATE_INITIALIZE_VAR, \
        validate_required
#define IMPL_VALIDATE_DECLARE_VAR_KIND_OPTIONAL_ENUM_CAPS(caps_seq)                 \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS, caps_seq, IMPL_VALIDATE_INITIALIZE_VAR, \
        validate_optional
#define IMPL_VALIDATE_DECLARE_VAR_KIND_ALLOWED_ONLY_IF_ENUM_CAPS(condition, caps_seq)         \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS, caps_seq, IMPL_VALIDATE_INITIALIZE_VAR_WITH_COND, \
        validate_allowed_only_if, condition
#define IMPL_VALIDATE_DECLARE_VAR_KIND_REQUIRED_AND_ALLOWED_ONLY_IF_ENUM_CAPS(condition, caps_seq) \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS, caps_seq, IMPL_VALIDATE_INITIALIZE_VAR_WITH_COND,      \
        validate_required_and_allowed_only_if, condition
#define IMPL_VALIDATE_DECLARE_VAR_KIND_FILE_REQUIRED \
    IMPL_VALIDATE_INITIALIZE_VAR, validate_file_required
#define IMPL_VALIDATE_DECLARE_VAR_KIND_FILE_OPTIONAL \
    IMPL_VALIDATE_INITIALIZE_VAR, validate_file_optional

#define IMPL_VALIDATE_DECLARE_VAR5(form_fields, errors_str, var_name, api_param, macro, ...) \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                         \
    auto var_name = macro(form_fields, errors_str, api_param, __VA_ARGS__);

#define IMPL_VALIDATE_INITIALIZE_VAR(form_fields, errors_str, api_param, func) \
    ::web_server::http::func(api_param, form_fields, errors_str)
#define IMPL_VALIDATE_INITIALIZE_VAR_WITH_COND(         \
    form_fields, errors_str, api_param, func, condition \
)                                                       \
    ::web_server::http::func(api_param, form_fields, errors_str, condition)

namespace detail {

template <class T>
struct stripped_optional {
    using stripped_type = T;
};

template <class T>
struct stripped_optional<std::optional<T>> : public stripped_optional<T> {};

template <class T>
using stripped_optional_t = typename stripped_optional<T>::stripped_type;

template <class T>
constexpr std::optional<stripped_optional_t<T>> flatten_optional(std::optional<T> opt) {
    if constexpr (std::is_same_v<stripped_optional_t<T>, T>) {
        return opt;
    } else {
        return opt ? flatten_optional(std::move(*opt)) : std::nullopt;
    }
}

} // namespace detail

#define IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS(                                         \
    form_fields, errors_str, api_param, caps_seq, macro, ...                            \
)                                                                                       \
    [&] {                                                                               \
        auto var = macro(form_fields, errors_str, api_param, __VA_ARGS__);              \
        using Enum = ::web_server::http::detail::stripped_optional_t<decltype(var)>;    \
        auto stripped_var = ::web_server::http::detail::flatten_optional(var);          \
        if (!stripped_var) {                                                            \
            return var;                                                                 \
        }                                                                               \
        /* NOLINTNEXTLINE(bugprone-switch-missing-default-case) */                      \
        switch (*stripped_var) {                                                        \
            REV_CAT(_END, IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_1 caps_seq)      \
        }                                                                               \
        ::web_server::http::detail::append_error(                                       \
            errors_str, api_param, "selects option to which you do not have permission" \
        );                                                                              \
        return decltype(var){std::nullopt};                                             \
    }()
#define IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_1_END
#define IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_2_END
#define IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_1(...)     \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_3(__VA_ARGS__) \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_2
#define IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_2(...)     \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_3(__VA_ARGS__) \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_1
#define IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_3(enum_variant, capability) \
    case Enum::enum_variant: {                                                   \
        if (capability) {                                                        \
            return var;                                                          \
        }                                                                        \
    } break;

} // namespace web_server::http
