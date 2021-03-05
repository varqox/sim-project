#pragma once

#include "sim/inf_datetime.hh"
#include "sim/sql_fields/varbinary.hh"

#include <string>

namespace sim::sql_fields {

// Format: YYYY-mm-dd HH:MM:SS | # | @
class InfDatetime : public Varbinary<std::char_traits<char>::length("YYYY-mm-dd HH:MM:SS")> {
public:
    InfDatetime() = default;
    constexpr InfDatetime(const InfDatetime&) = default;
    InfDatetime(InfDatetime&&) noexcept = default;
    InfDatetime& operator=(const InfDatetime&) = default;
    InfDatetime& operator=(InfDatetime&&) noexcept = default;
    ~InfDatetime() override = default;

    explicit InfDatetime(const sim::InfDatetime& dt)
    : Varbinary(dt.to_str()) {}

    explicit InfDatetime(sim::InfDatetime&& dt)
    : Varbinary(dt.to_str()) {}

    template <
        class T,
        std::enable_if_t<
            std::is_convertible_v<T, StringView> and
                !std::is_same_v<std::decay_t<T>, InfDatetime>,
            int> = 0>
    // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
    explicit InfDatetime(T&& str)
    : InfDatetime(sim::InfDatetime(std::forward<T>(str))) {}

    InfDatetime& operator=(const sim::InfDatetime& dt) {
        Varbinary::operator=(dt.to_str());
        return *this;
    }

    InfDatetime& operator=(sim::InfDatetime&& dt) {
        Varbinary::operator=(dt.to_str());
        return *this;
    }

    template <class T, std::enable_if_t<std::is_convertible_v<T, StringView>, int> = 0>
    InfDatetime& operator=(T&& str) {
        *this = sim::InfDatetime(std::forward<T>(str));
        return *this;
    }

    sim::InfDatetime as_inf_datetime() { return sim::InfDatetime(to_cstr()); }

    [[nodiscard]] sim::InfDatetime as_inf_datetime() const { return sim::InfDatetime(*this); }

    // NOLINTNEXTLINE(google-explicit-constructor)
    operator sim::InfDatetime() { return as_inf_datetime(); }
    // NOLINTNEXTLINE(google-explicit-constructor)
    operator sim::InfDatetime() const { return as_inf_datetime(); }
};

} // namespace sim::sql_fields
