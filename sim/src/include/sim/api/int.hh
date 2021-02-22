#pragma once

#include "simlib/concat_tostr.hh"
#include "simlib/string_transform.hh"
#include "simlib/string_view.hh"
#include "simlib/to_string.hh"

#include <limits>
#include <variant>

namespace sim::api {

// Represents an integral value of type @p IntT from [@p min_val, @p max_val]
template <
    class IntT, const char* const var_name, const char* const var_description,
    IntT min_val = std::numeric_limits<IntT>::min(),
    IntT max_val = std::numeric_limits<IntT>::max()>
class Int {
public:
    using int_type = IntT;

    IntT val;

    constexpr Int() = default;
    ~Int() = default;
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Int(IntT v)
    : val{v} {}

    constexpr Int(const Int&) noexcept = default;
    constexpr Int(Int&&) noexcept = default;
    constexpr Int& operator=(const Int&) noexcept = default;
    constexpr Int& operator=(Int&&) noexcept = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    [[nodiscard]] operator const IntT&() const noexcept { return val; }

    // NOLINTNEXTLINE(google-explicit-constructor)
    [[nodiscard]] operator IntT&() noexcept { return val; }

    constexpr Int& operator=(IntT v) noexcept {
        val = v;
        return *this;
    }

    friend auto stringify(const Int& i) noexcept { return stringify(i.val); }
    friend auto stringify(Int& i) noexcept { return stringify(i.val); }
    friend auto stringify(Int&& i) noexcept { return stringify(i.val); }

    static inline const CStringView api_var_name = CStringView{var_name};
    static inline const CStringView api_var_description = CStringView{var_description};

    // Returns parsed object or error description
    [[nodiscard]] static std::variant<Int, std::string> from_str(StringView str) {
        auto opt = str2num<IntT>(str, min_val, max_val);
        if (not opt) {
            return concat_tostr(
                api_var_description, " (", api_var_name, "): value not in range [", min_val,
                ", ", max_val, "]");
        }
        return *opt;
    }

    [[nodiscard]] constexpr auto to_str() const noexcept { return to_string(val); }

    [[nodiscard]] auto to_json_value() const { return to_str(); }
};

} // namespace sim::api
