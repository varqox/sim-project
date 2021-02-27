#pragma once

#include "simlib/concat_tostr.hh"
#include "simlib/string_view.hh"

#include <cstddef>
#include <variant>

namespace sim::web_api {

// Represents an integral value of type @p IntT from [@p min_val, @p max_val]
template <
    class StringT, const char* const var_name, const char* const var_description,
    size_t max_len = StringT::max_len>
class String : public StringT {
public:
    using StringT::StringT;

    static inline const CStringView api_var_name = CStringView{var_name};
    static inline const CStringView api_var_description = CStringView{var_description};

    // Needs to be implemented alongside specialization
    // Returns parsed object or error description
    [[nodiscard]] static std::variant<String, std::string> from_str(StringView str) {
        if (str.size() > max_len) {
            return concat_tostr(
                api_var_description, " (", api_var_name, ") cannot be longer than ", max_len,
                " bytes");
        }
        return String{str};
    }

    // Needs to be implemented alongside specialization
    [[nodiscard]] constexpr const auto& to_str() const noexcept { return *this; }

    [[nodiscard]] auto to_json_value() const { return json_stringify(to_str()); }
};

} // namespace sim::web_api
