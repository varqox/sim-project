#pragma once

#include "simlib/string_view.hh"
#include "varchar_field.hh"

namespace sim {

// Format: YYYY-mm-dd HH:MM:SS
class DatetimeField : public VarcharField<19> {
public:
    DatetimeField() = default;
    constexpr DatetimeField(const DatetimeField&) = default;
    DatetimeField(DatetimeField&&) = default;
    DatetimeField& operator=(const DatetimeField&) = default;
    DatetimeField& operator=(DatetimeField&&) noexcept = default;
    ~DatetimeField() override = default;

    template <
        class T,
        std::enable_if_t<
            std::is_convertible_v<T, StringView> and
                !std::is_same_v<std::decay_t<T>, DatetimeField>,
            int> = 0>
    // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
    constexpr explicit DatetimeField(T&& str)
    : VarcharField([&]() -> decltype(auto) {
        throw_assert(is_datetime(intentional_unsafe_c_string_view(concat(str))));
        return str;
    }()) {}

    template <
        class T,
        std::enable_if_t<
            std::is_convertible_v<T, StringView> and
                !std::is_same_v<std::decay_t<T>, DatetimeField>,
            int> = 0>
    DatetimeField& operator=(T&& str) {
        throw_assert(is_datetime(intentional_unsafe_c_string_view(concat(str))));
        VarcharField::operator=(std::forward<T>(str));
        return *this;
    }
};

} // namespace sim
