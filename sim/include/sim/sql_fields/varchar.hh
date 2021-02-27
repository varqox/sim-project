#pragma once

#include "simlib/inplace_buff.hh"

namespace sim::sql_fields {

template <size_t MAX_LEN>
class Varchar : public InplaceBuff<MAX_LEN + 1> { // +1 for a potential null byte
public:
    using StrType = InplaceBuff<MAX_LEN + 1>;
    static constexpr size_t max_len = MAX_LEN;

    using StrType::StrType;
    using StrType::operator=;

    constexpr Varchar(const Varchar&) = default;
    constexpr Varchar(Varchar&&) noexcept = default;
    constexpr Varchar& operator=(const Varchar&) = default;
    constexpr Varchar& operator=(Varchar&&) noexcept = default;
    ~Varchar() = default;
};

} // namespace sim::sql_fields
