#pragma once

#include <sim/sql_fields/varbinary.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/string_view.hh>
#include <string>

namespace sim::sql_fields {

// Format: YYYY-mm-dd HH:MM:SS
class Datetime : public Varbinary<std::char_traits<char>::length("YYYY-mm-dd HH:MM:SS")> {
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
            std::is_convertible_v<T, StringView> and !std::is_same_v<std::decay_t<T>, Datetime>,
            int> = 0>
    // NOLINTNEXTLINE(bugprone-forwarding-reference-overload)
    constexpr explicit Datetime(T&& str)
    : Varbinary([&]() -> decltype(auto) {
        throw_assert(is_datetime(from_unsafe{concat(str)}));
        return str;
    }()) {}

    template <
        class T,
        std::enable_if_t<
            std::is_convertible_v<T, StringView> and !std::is_same_v<std::decay_t<T>, Datetime>,
            int> = 0>
    Datetime& operator=(T&& str) {
        throw_assert(is_datetime(from_unsafe{concat(str)}));
        Varbinary::operator=(std::forward<T>(str));
        return *this;
    }

    [[nodiscard]] std::string to_json() const {
        auto self = StringView{*this};
        auto date = self.extract_prefix(std::char_traits<char>::length("YYYY-mm-dd"));
        self.remove_prefix(1);
        auto time = self;
        return concat_tostr('"', date, 'T', time, R"(Z")");
    }
};

} // namespace sim::sql_fields
