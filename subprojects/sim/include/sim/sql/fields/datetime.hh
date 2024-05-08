#pragma once

#include "varbinary.hh"

#include <simlib/concat_tostr.hh>
#include <simlib/throw_assert.hh>
#include <simlib/time.hh>
#include <string>
#include <string_view>

namespace sim::sql::fields {

// Format: YYYY-mm-dd HH:MM:SS
class Datetime : public Varbinary<std::char_traits<char>::length("YYYY-mm-dd HH:MM:SS")> {
public:
    Datetime() noexcept = default;
    Datetime(const Datetime&) = default;
    Datetime(Datetime&&) noexcept = default;
    Datetime& operator=(const Datetime&) = default;
    Datetime& operator=(Datetime&&) noexcept = default;
    ~Datetime() = default;

    explicit Datetime(std::string str)
    : Varbinary{[&]() -> decltype(auto) {
        throw_assert(is_datetime(str));
        return std::move(str);
    }()} {}

    Datetime& operator=(std::string str) {
        throw_assert(is_datetime(str));
        Varbinary::operator=(std::move(str));
        return *this;
    }

    [[nodiscard]] std::string to_json() const {
        auto sv = std::string_view{*this};
        auto date = sv.substr(0, std::char_traits<char>::length("YYYY-mm-dd"));
        auto time = sv.substr(date.size() + 1);
        return concat_tostr('"', date, 'T', time, "Z\"");
    }
};

} // namespace sim::sql::fields
