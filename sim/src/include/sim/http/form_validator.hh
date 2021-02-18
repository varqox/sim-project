#pragma once

#include "sim/http/form_fields.hh"
#include "simlib/always_false.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/string_transform.hh"

#include <string>
#include <type_traits>
#include <variant>

namespace http {

class FormValidator {
    std::string errors_;
    const FormFields& form_fields_;

    static inline const std::string empty_string_{};

public:
    explicit FormValidator(const FormFields& form_fields)
    : form_fields_(form_fields) {}

    template <class... Args>
    void add_error(Args&&... message) {
        back_insert(
            errors_, "<pre class=\"error\">", std::forward<Args>(message)..., "</pre>\n");
    }

    [[nodiscard]] bool has_errors() const noexcept { return not errors_.empty(); }

    [[nodiscard]] const auto& errors() const noexcept { return errors_; }

private:
    template <class T, std::enable_if_t<std::is_default_constructible_v<T>, int> = 0>
    T validate_impl(bool allow_blank) {
        const std::string* value = form_fields_.get(T::api_var_name);
        if (not value) {
            add_error(html_escape(
                concat_tostr(T::api_var_description, " (", T::api_var_name, ") is not set")));
            return {};
        }
        if (value->empty() and not allow_blank) {
            add_error(html_escape(concat_tostr(
                T::api_var_description, " (", T::api_var_name, ") cannot be blank")));
            return {};
        }
        return std::visit(
            [&](auto val) -> T {
                using ValT = decltype(val);
                if constexpr (std::is_same_v<ValT, T>) {
                    return val;
                } else if constexpr (std::is_same_v<ValT, std::string>) {
                    add_error(html_escape(std::move(val)));
                    return {};
                } else {
                    static_assert(always_false<ValT>, "Missing branch");
                }
            },
            T::from_str(*value));
    }

public:
    template <class T, std::enable_if_t<std::is_default_constructible_v<T>, int> = 0>
    T validate() {
        return validate_impl<T>(false);
    }

    template <class T, std::enable_if_t<std::is_default_constructible_v<T>, int> = 0>
    T validate_allow_blank() {
        return validate_impl<T>(true);
    }

private:
    template <class T, class C>
    static T member_object_type(T C::*);

public:
    template <auto ptr>
    decltype(member_object_type(ptr)) validate() {
        return validate<decltype(member_object_type(ptr))>();
    }

    template <auto ptr>
    decltype(member_object_type(ptr)) validate_allow_blank() {
        return validate_allow_blank<decltype(member_object_type(ptr))>();
    }
};

} // namespace http
