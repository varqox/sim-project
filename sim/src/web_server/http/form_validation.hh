#pragma once

#include "sim/sql_fields/blob.hh"
#include "sim/sql_fields/bool.hh"
#include "sim/sql_fields/satisfying_predicate.hh"
#include "sim/sql_fields/varbinary.hh"
#include "simlib/always_false.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/enum_val.hh"
#include "simlib/enum_with_string_conversions.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/form_fields.hh"

#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace web_server::http {

template <class T>
struct ApiParam {
    CStringView name;
    CStringView description;

    using type = T;

    constexpr ApiParam(CStringView name, CStringView description) noexcept
    : name{name}
    , description{description} {}

    template <
        class OtherT, std::enable_if_t<not std::is_same_v<T, std::decay_t<OtherT>>, int> = 0>
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr ApiParam(ApiParam<OtherT> api_param) noexcept
    : ApiParam{api_param.name, api_param.description} {}

    template <class C>
    constexpr ApiParam(T C::* /*unused*/, CStringView name, CStringView description) noexcept
    : ApiParam{name, description} {}
};

namespace detail {

template <class T>
auto validate_result_type() {
    if constexpr (sim::sql_fields::is_satisfying_predicate<T>) {
        return T{validate_result_type<typename T::underlying_type>()};
    } else if constexpr (std::is_same_v<T, bool> or std::is_same_v<T, sim::sql_fields::Bool>) {
        return false;
    } else if constexpr (
        is_enum_val_with_string_conversions<T> or std::is_integral_v<T> or
        sim::sql_fields::is_varbinary<T> or sim::sql_fields::is_blob<T> or
        std::is_same_v<T, CStringView>)
    {
        return T{};
    } else {
        static_assert(always_false<T>, "Unsupported type");
    }
}

template <class T, class... Arg>
std::nullopt_t append_error(std::string& errors_str, ApiParam<T> api_param, Arg&&... args) {
    if (!errors_str.empty()) {
        errors_str += '\n';
    }
    back_insert(
        errors_str, api_param.name, ": ", api_param.description, ' ',
        std::forward<decltype(args)>(args)...);
    return std::nullopt;
}

} // namespace detail

template <class T>
std::optional<decltype(detail::validate_result_type<T>())> validate(
    const FormFields& form_fields, std::string& errors_str, ApiParam<T> api_param,
    bool allow_blank) {
    auto error = [&](auto&&... args) {
        return detail::append_error(
            errors_str, api_param, std::forward<decltype(args)>(args)...);
    };
    if constexpr (sim::sql_fields::is_satisfying_predicate<T>) {
        auto opt = validate<typename T::underlying_type>(
            form_fields, errors_str, api_param, allow_blank);
        if (!opt) {
            return std::nullopt;
        }
        if (not T::predicate(T{*opt})) {
            error(T::description);
            return std::nullopt;
        }
        return std::optional{T{std::move(*opt)}};
    } else if constexpr (std::is_same_v<T, bool> or std::is_same_v<T, sim::sql_fields::Bool>) {
        return form_fields.contains(api_param.name);
    } else {
        const auto str_val_opt = form_fields.get(api_param.name);
        if (not str_val_opt) {
            return error("is not set");
        }
        const auto& str_val = *str_val_opt;
        if (not allow_blank and str_val.empty()) {
            return error("cannot be blank");
        }
        // Parse str_val
        if constexpr (is_enum_val_with_string_conversions<T>) {
            if (auto opt = T::EnumType::from_str(str_val); opt) {
                return std::move(*opt);
            }
            return error("has invalid value");
        } else if constexpr (std::is_integral_v<T>) {
            if (auto opt = str2num<T>(str_val); opt) {
                return std::move(*opt);
            }
            static constexpr auto min_val = std::numeric_limits<T>::min();
            static constexpr auto max_val = std::numeric_limits<T>::max();
            return error("is not in range [", min_val, ", ", max_val, ']');
        } else if constexpr (sim::sql_fields::is_varbinary<T>) {
            if (str_val.size() <= T::max_len) {
                return T{str_val};
            }
            return error("cannot be longer than ", T::max_len, " bytes");
        } else if constexpr (std::is_same_v<T, CStringView> or sim::sql_fields::is_blob<T>) {
            return T{str_val};
        } else {
            static_assert(always_false<T>, "Unsupported type");
        }
    }
}

#define VALIDATE(form_fields, errors_str_to_return_value_func, seq) \
    IMPL_VALIDATE(                                                  \
        form_fields, errors_str_to_return_value_func,               \
        CAT(_validate_variant_macro_local_variable_, __COUNTER__),  \
        MAP(IMPL_VALIDATE_VAR_NAMES, seq), seq)
#define IMPL_VALIDATE_VAR_NAMES(var_name, ...) (var_name)
#define IMPL_VALIDATE(...) IMPL_VALIDATE2(__VA_ARGS__)
#define IMPL_VALIDATE2(                                                                     \
    form_fields, errors_str_to_return_value_func, validate_variant_var, var_names_seq, seq) \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                        \
    auto validate_variant_var =                                                             \
        [&] { /* NOLINT(bugprone-macro-parentheses) */                                      \
              std::string errors_str;                                                       \
              IMPL_VALIDATE_DECLARE_VARS(seq, form_fields, errors_str)                      \
              using TupleType = decltype(std::tuple{MAP_DELIM_FUNC(                         \
                  IMPL_VALIDATE_VAR_NAME_TO_STAR_VAR_NAME, COMMA, var_names_seq)});         \
              if (MAP_DELIM(IMPL_VALIDATE_VAR_NAME_TO_NOT_VAR_NAME, ||, var_names_seq)) {   \
                  return std::variant<TupleType, std::string>{                              \
                      std::in_place_type<std::string>, std::move(errors_str)};              \
              }                                                                             \
              return std::variant<TupleType, std::string>{                                  \
                  std::in_place_type<TupleType>,                                            \
                  MAP_DELIM_FUNC(                                                           \
                      IMPL_VALIDATE_VAR_NAME_TO_STAR_VAR_NAME, COMMA, var_names_seq)};      \
        }();                                                                                \
    if (auto* validation_errors_str = std::get_if<1>(&(validate_variant_var)))              \
    { /* NOLINT(bugprone-macro-parentheses) */                                              \
        return errors_str_to_return_value_func(*validation_errors_str);                     \
    }                                                                                       \
    auto& [MAP_DELIM_FUNC(IMPL_VALIDATE_VAR_NAME, COMMA, var_names_seq)] =                  \
        std::get<0>(validate_variant_var)
#define IMPL_VALIDATE_VAR_NAME(var_name) var_name
#define IMPL_VALIDATE_VAR_NAME_TO_STAR_VAR_NAME(var_name) *var_name
#define IMPL_VALIDATE_VAR_NAME_TO_NOT_VAR_NAME(var_name) !var_name
#define IMPL_VALIDATE_DECLARE_VARS(seq, form_fields, errors_str) \
    IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES(                       \
        FOLDR(IMPL_VALIDATE_DECLARE_VAR, seq, form_fields, errors_str, ))
#define IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES(...) \
    IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES2(__VA_ARGS__)
#define IMPL_VALIDATE_DECLARE_VAR_EXTRACT_RES2(form_fields, errors_str, ...) __VA_ARGS__
#define IMPL_VALIDATE_DECLARE_VAR(args, form_fields, errors_str, ...) \
    form_fields, errors_str,                                          \
        IMPL_VALIDATE_DECLARE_VAR2(                                   \
            form_fields, errors_str, IMPL_VALIDATE_DECLARE_VAR_NOOP args, ) __VA_ARGS__
#define IMPL_VALIDATE_DECLARE_VAR_NOOP(...) __VA_ARGS__
#define IMPL_VALIDATE_DECLARE_VAR2(...) IMPL_VALIDATE_DECLARE_VAR3(__VA_ARGS__, )
#define IMPL_VALIDATE_DECLARE_VAR3(form_fields, errors_str, var_name, api_param, option, ...) \
    IMPL_VALIDATE_DECLARE_VAR4(                                                               \
        form_fields, errors_str, var_name, api_param,                                         \
        PRIMITIVE_DOUBLE_CAT(IMPL_VALIDATE_DECLARE_VAR_OPTION, _, option))
#define IMPL_VALIDATE_DECLARE_VAR4(...) IMPL_VALIDATE_DECLARE_VAR5(__VA_ARGS__)

#define IMPL_VALIDATE_DECLARE_VAR_OPTION_ IMPL_VALIDATE_INITIALIZE_VAR_SIMPLE, false
#define IMPL_VALIDATE_DECLARE_VAR_OPTION_ALLOW_BLANK IMPL_VALIDATE_INITIALIZE_VAR_SIMPLE, true
#define IMPL_VALIDATE_DECLARE_VAR_OPTION_ALLOW_IF(condition)                               \
    IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF, condition, IMPL_VALIDATE_INITIALIZE_VAR_SIMPLE, \
        (, false)
#define IMPL_VALIDATE_DECLARE_VAR_OPTION_ALLOW_BLANK_ALLOW_IF(condition)                   \
    IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF, condition, IMPL_VALIDATE_INITIALIZE_VAR_SIMPLE, \
        (, true)
#define IMPL_VALIDATE_DECLARE_VAR_OPTION_ENUM_CAPS(caps_seq) \
    IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS, caps_seq
#define IMPL_VALIDATE_DECLARE_VAR_OPTION_ENUM_CAPS_ALLOW_IF(condition, caps_seq)              \
    IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF, condition, IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS, \
        (, caps_seq)

#define IMPL_VALIDATE_DECLARE_VAR5(form_fields, errors_str, var_name, api_param, macro, ...) \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                                         \
    auto var_name = macro(form_fields, errors_str, api_param, __VA_ARGS__);

#define IMPL_VALIDATE_INITIALIZE_VAR_SIMPLE(form_fields, errors_str, api_param, allow_blank) \
    ::web_server::http::validate(form_fields, errors_str, api_param, allow_blank)

#define IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF(                                 \
    form_fields, errors_str, api_param, condition, macro, macro_extra_args)    \
    [&] {                                                                      \
        auto&& form_fields_var = form_fields;                                  \
        auto&& api_param_var = api_param;                                      \
        auto& errors_str_var = errors_str;                                     \
        auto do_validate = [&] {                                               \
            return IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF_CALL_MACRO(           \
                macro, form_fields_var, errors_str_var,                        \
                api_param_var IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF_PASTE_ARGS \
                    macro_extra_args);                                         \
        };                                                                     \
        using ResType = std::optional<decltype(do_validate())>;                \
        if (!(condition)) {                                                    \
            if (form_fields_var.contains(api_param_var.name)) {                \
                ::web_server::http::detail::append_error(                      \
                    errors_str_var, api_param_var,                             \
                    "should not be sent within request at all");               \
                return ResType{std::nullopt};                                  \
            }                                                                  \
            return ResType{decltype(do_validate()){std::nullopt}};             \
        }                                                                      \
        auto res = do_validate();                                              \
        if (!res) {                                                            \
            return ResType{std::nullopt};                                      \
        }                                                                      \
        return ResType{std::move(res)};                                        \
    }()

#define IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF_PASTE_ARGS(...) __VA_ARGS__
#define IMPL_VALIDATE_INITIALIZE_VAR_ALLOW_IF_CALL_MACRO(macro, ...) macro(__VA_ARGS__)

#define IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS(form_fields, errors_str, api_param, caps_seq) \
    [&] {                                                                                    \
        auto var =                                                                           \
            IMPL_VALIDATE_INITIALIZE_VAR_SIMPLE(form_fields, errors_str, api_param, false);  \
        if (!var) {                                                                          \
            return decltype(var){std::nullopt};                                              \
        }                                                                                    \
        using Enum = std::remove_reference_t<decltype(*var)>::EnumType;                      \
        switch (*var) {                                                                      \
            REV_CAT(_END, IMPL_VALIDATE_INITIALIZE_VAR_ENUM_CAPS_CASES_1 caps_seq)           \
        }                                                                                    \
        ::web_server::http::detail::append_error(                                            \
            errors_str, api_param, "selects option to which you do not have permission");    \
        return decltype(var){std::nullopt};                                                  \
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
