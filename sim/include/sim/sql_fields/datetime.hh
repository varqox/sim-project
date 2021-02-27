#pragma once

#include "sim/sql_fields/varchar.hh"
#include "simlib/string_view.hh"

#include <string>

namespace sim::sql_fields {

// Format: YYYY-mm-dd HH:MM:SS
class Datetime : public Varchar<std::char_traits<char>::length("YYYY-mm-dd HH:MM:SS")> {
public:
    Datetime() = default;
    constexpr Datetime(const Datetime&) = default;
    Datetime(Datetime&&) = default;
    Datetime& operator=(const Datetime&) = default;
    Datetime& operator=(Datetime&&) noexcept = default;
    ~Datetime() override = default;

    template <
        class T,
        std::enable_if_t<
            std::is_convertible_v<T, StringView> and
                !std::is_same_v<std::decay_t<T>, Datetime>,
            int> = 0>
    // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
    constexpr explicit Datetime(T&& str)
    : Varchar([&]() -> decltype(auto) {
        throw_assert(is_datetime(intentional_unsafe_c_string_view(concat(str))));
        return str;
    }()) {}

    template <
        class T,
        std::enable_if_t<
            std::is_convertible_v<T, StringView> and
                !std::is_same_v<std::decay_t<T>, Datetime>,
            int> = 0>
    Datetime& operator=(T&& str) {
        throw_assert(is_datetime(intentional_unsafe_c_string_view(concat(str))));
        Varchar::operator=(std::forward<T>(str));
        return *this;
    }
};

} // namespace sim::sql_fields
