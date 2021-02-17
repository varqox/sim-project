#pragma once

#include "sim/http/form_fields.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/string_transform.hh"

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

    // Returned value may be used only after checking that has_errors() == false
    template <class Checker>
    const std::string& validate(
        StringView name, StringView name_to_print, Checker&& checker,
        StringView checker_fail_msg = ": validation error",
        size_t max_size = std::numeric_limits<size_t>::max()) {
        static_assert(std::is_invocable_r_v<bool, Checker, const std::string&>);

        const std::string* value = form_fields_.get(name);
        if (not value) {
            add_error(html_escape(name_to_print), " is not set");
            return empty_string_;
        }

        if (value->size() > max_size) {
            add_error(
                html_escape(name_to_print), " cannot be longer than ", max_size, " bytes");
            return empty_string_;
        }

        if (not checker(*value)) {
            add_error(html_escape(name_to_print), html_escape(checker_fail_msg));
            return empty_string_;
        }

        return *value;
    }

    // Returned value may be used only after checking that has_errors() == false
    const std::string& validate(
        StringView name, StringView name_to_print,
        size_t max_size = std::numeric_limits<size_t>::max()) {
        return validate(
            name, name_to_print, [](auto&& /*unused*/) { return true; }, "", max_size);
    }

    // Returned value may be used only after checking that has_errors() ==
    // false
    template <class T>
    T validate_enum(StringView name, StringView name_to_print) {
        const std::string* value = form_fields_.get(name);
        if (not value) {
            add_error(html_escape(name_to_print), " is not set");
            return T{};
        }

        std::optional<T> opt = T::from_str(*value);
        if (not opt) {
            add_error("Invalid ", html_escape(name_to_print));
            return T{};
        }

        return *opt;
    }
};

} // namespace http
