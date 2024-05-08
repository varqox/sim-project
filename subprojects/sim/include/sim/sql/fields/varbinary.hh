#pragma once

#include <string>

namespace sim::sql::fields {

template <size_t MAX_LEN>
class Varbinary : public std::string {
public:
    static constexpr size_t max_len = MAX_LEN;

    using std::string::string;
    using std::string::operator=;

    // NOLINTNEXTLINE(google-explicit-constructor)
    Varbinary(std::string&& str) : std::string{std::move(str)} {}

    Varbinary(const Varbinary&) = default;
    Varbinary(Varbinary&&) noexcept = default;
    Varbinary& operator=(const Varbinary&) = default;
    Varbinary& operator=(Varbinary&&) noexcept = default;
    ~Varbinary() = default;
};

} // namespace sim::sql::fields
