#pragma once

#include "sim/sql_fields/varbinary.hh"
#include "simlib/always_false.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/enum_val.hh"
#include "simlib/enum_with_string_conversions.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"
#include "src/web_server/http/form_fields.hh"

#include <limits>
#include <string>
#include <type_traits>
#include <variant>

namespace web_server::http {

template <class T>
struct ApiParam {
    CStringView name;
    CStringView description;

    constexpr ApiParam(CStringView name, CStringView description) noexcept
    : name{name}
    , description{description} {}

    template <class C>
    constexpr ApiParam(T C::* /*unused*/, CStringView name, CStringView description) noexcept
    : ApiParam{name, description} {}
};

namespace detail {

template <class T>
std::variant<T, std::string>
validate_impl(const FormFields& form_fields, ApiParam<T> api_param, bool allow_blank) {
    const auto* str_val = form_fields.get(api_param.name);
    if (not str_val) {
        return concat_tostr(api_param.name, ": ", api_param.description, " is not set");
    }
    if (str_val->empty() and not allow_blank) {
        return concat_tostr(api_param.name, ": ", api_param.description, " cannot be blank");
    }
    // Parse str_val
    if constexpr (is_enum_val_with_string_conversions<T>) {
        if (auto opt = T::EnumType::from_str(*str_val); opt) {
            return std::move(*opt);
        }
        return concat_tostr(api_param.name, ": ", api_param.description, " has invalid value");
    } else if constexpr (std::is_integral_v<T>) {
        if (auto opt = str2num<T>(*str_val); opt) {
            return std::move(*opt);
        }
        static constexpr auto min_val = std::numeric_limits<T>::min();
        static constexpr auto max_val = std::numeric_limits<T>::max();
        return concat_tostr(
            api_param.name, ": ", api_param.description, " is not in range [", min_val, ", ",
            max_val, ']');
    } else if constexpr (sim::sql_fields::is_varbinary<T>) {
        if (str_val->size() <= T::max_len) {
            return T{*str_val};
        }
        return concat_tostr(
            api_param.name, ": ", api_param.description, " cannot be longer than ", T::max_len,
            " bytes");
    } else {
        static_assert(always_false<T>, "Unsupported type");
    }
}

} // namespace detail

template <class T>
std::variant<T, std::string> validate(const FormFields& form_fields, ApiParam<T> api_param) {
    return detail::validate_impl(form_fields, api_param, false);
}

template <class T>
std::variant<T, std::string>
validate_allow_blank(const FormFields& form_fields, ApiParam<T> api_param) {
    return detail::validate_impl(form_fields, api_param, true);
}

} // namespace web_server::http
