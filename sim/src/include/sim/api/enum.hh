#pragma once

#include "simlib/concat_tostr.hh"
#include "simlib/enum_val.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"

#include <optional>
#include <variant>

namespace sim::api {

template <class EnumT>
class Enum : public EnumVal<EnumT> {
public:
    using EnumVal<EnumT>::EnumVal;

    // Needs to be implemented alongside specialization
    constexpr static CStringView api_var_name;
    constexpr static CStringView api_var_description;

    constexpr static std::optional<Enum> from_str_impl(StringView str);

    // Needs to be implemented alongside specialization
    // Returns parsed object or error description
    static std::variant<Enum, std::string> from_str(StringView str) {
        if (auto opt = from_str_impl(str)) {
            return *opt;
        }
        return concat_tostr(api_var_description, " (", api_var_name, "): invalid value");
    }

    // Needs to be implemented alongside specialization
    [[nodiscard]] constexpr CStringView to_str() const noexcept;

    [[nodiscard]] auto to_json_value() const { return json_stringify(to_str()); }
};

} // namespace sim::api
