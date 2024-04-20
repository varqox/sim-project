#pragma once

#include <string>

namespace sim::sql::fields {

class Blob : public std::string {
public:
    using std::string::string;
    using std::string::operator=;

    // NOLINTNEXTLINE(google-explicit-constructor)
    Blob(std::string&& str) : std::string{std::move(str)} {}

    Blob(const Blob&) = default;
    Blob(Blob&&) noexcept = default;
    Blob& operator=(const Blob&) = default;
    Blob& operator=(Blob&&) noexcept = default;
    ~Blob() = default;
};

} // namespace sim::sql::fields
