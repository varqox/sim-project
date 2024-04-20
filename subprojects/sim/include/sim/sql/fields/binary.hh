#pragma once

#include <string>

namespace sim::sql::fields {

template <size_t LEN>
class Binary : public std::string {
public:
    static constexpr size_t len = LEN;

    using std::string::string;
    using std::string::operator=;

    // NOLINTNEXTLINE(google-explicit-constructor)
    Binary(std::string&& str) : std::string{std::move(str)} {}

    Binary(const Binary&) = default;
    Binary(Binary&&) noexcept = default;
    Binary& operator=(const Binary&) = default;
    Binary& operator=(Binary&&) noexcept = default;
    ~Binary() = default;
};

} // namespace sim::sql::fields
