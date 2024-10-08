#pragma once

#include "binary.hh"

#include <simlib/concat_tostr.hh>
#include <simlib/throw_assert.hh>
#include <simlib/time.hh>
#include <string>
#include <string_view>

namespace sim::sql::fields {

// Format: YYYY-mm-dd HH:MM:SS | # | @
class InfDatetime : public Binary<std::char_traits<char>::length("YYYY-mm-dd HH:MM:SS")> {
    static constexpr const auto NEG_INF_STR = std::string_view{"#"};
    static constexpr const auto INF_STR = std::string_view{"@"};

public:
    InfDatetime() noexcept = default;
    InfDatetime(const InfDatetime&) = default;
    InfDatetime(InfDatetime&&) noexcept = default;
    InfDatetime& operator=(const InfDatetime&) = default;
    InfDatetime& operator=(InfDatetime&&) noexcept = default;
    ~InfDatetime() = default;

    explicit InfDatetime(std::string str)
    : Binary{[&]() -> decltype(auto) {
        // Database stores binary as bytes padded with '\0' at the end
        while (!str.empty() && str.back() == '\0') {
            str.pop_back();
        }
        throw_assert(str == NEG_INF_STR || str == INF_STR || is_datetime(str.c_str()));
        return std::move(str);
    }()} {}

    InfDatetime& operator=(std::string str) {
        // Database stores binary as bytes padded with '\0' at the end
        while (!str.empty() && str.back() == '\0') {
            str.pop_back();
        }
        throw_assert(str == NEG_INF_STR || str == INF_STR || is_datetime(str.c_str()));
        Binary::operator=(std::move(str));
        return *this;
    }

    [[nodiscard]] bool is_neg_inf() const noexcept {
        return std::string_view{*this} == NEG_INF_STR;
    }

    [[nodiscard]] bool is_inf() const noexcept { return std::string_view{*this} == INF_STR; }

    [[nodiscard]] std::string to_json() const {
        if (is_neg_inf()) {
            return "-inf";
        }
        if (is_inf()) {
            return "+inf";
        }
        auto sv = std::string_view{*this};
        auto date = sv.substr(0, std::char_traits<char>::length("YYYY-mm-dd"));
        auto time = sv.substr(date.size() + 1);
        return concat_tostr('"', date, 'T', time, "Z\"");
    }
};

} // namespace sim::sql::fields
