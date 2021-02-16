#pragma once

#include "sim/inf_datetime.hh"
#include "sim/varchar_field.hh"

namespace sim {

// Format: YYYY-mm-dd HH:MM:SS | # | @
class InfDatetimeField : public VarcharField<19> {
public:
    InfDatetimeField() = default;
    constexpr InfDatetimeField(const InfDatetimeField&) = default;
    InfDatetimeField(InfDatetimeField&&) noexcept = default;
    InfDatetimeField& operator=(const InfDatetimeField&) = default;
    InfDatetimeField& operator=(InfDatetimeField&&) noexcept = default;
    ~InfDatetimeField() override = default;

    explicit InfDatetimeField(const InfDatetime& dt)
    : VarcharField(dt.to_str()) {}

    explicit InfDatetimeField(InfDatetime&& dt)
    : VarcharField(dt.to_str()) {}

    template <
        class T,
        std::enable_if_t<
            std::is_convertible_v<T, StringView> and
                !std::is_same_v<std::decay_t<T>, InfDatetimeField>,
            int> = 0>
    // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
    explicit InfDatetimeField(T&& str)
    : InfDatetimeField(InfDatetime(std::forward<T>(str))) {}

    InfDatetimeField& operator=(const InfDatetime& dt) {
        VarcharField::operator=(dt.to_str());
        return *this;
    }

    InfDatetimeField& operator=(InfDatetime&& dt) {
        VarcharField::operator=(dt.to_str());
        return *this;
    }

    template <class T, std::enable_if_t<std::is_convertible_v<T, StringView>, int> = 0>
    InfDatetimeField& operator=(T&& str) {
        *this = InfDatetime(std::forward<T>(str));
        return *this;
    }

    InfDatetime as_inf_datetime() { return InfDatetime(to_cstr()); }

    [[nodiscard]] InfDatetime as_inf_datetime() const { return InfDatetime(*this); }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator InfDatetime() { return as_inf_datetime(); }
    // NOLINTNEXTLINE(google-explicit-constructor)
    operator InfDatetime() const { return as_inf_datetime(); }
};

} // namespace sim
