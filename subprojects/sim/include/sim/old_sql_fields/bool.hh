#pragma once

#include <cstdint>

namespace sim::old_sql_fields {

class Bool {
public:
    using int_type = uint8_t;

private:
    int_type val = false;

public:
    constexpr Bool() = default;

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Bool(bool val) : val{val} {}

    constexpr Bool(const Bool&) = default;
    constexpr Bool(Bool&&) noexcept = default;
    constexpr Bool& operator=(const Bool&) = default;
    constexpr Bool& operator=(Bool&&) noexcept = default;
    ~Bool() = default;

    constexpr Bool& operator=(bool other) noexcept {
        val = other;
        return *this;
    }

    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator bool() const noexcept { return val; }

    constexpr explicit operator int_type&() noexcept { return val; }
};

} // namespace sim::old_sql_fields
