#pragma once

#include "simlib/inplace_buff.hh"

namespace sim::sql_fields {

template <size_t STATIC_LEN = 32>
class Blob : public InplaceBuff<STATIC_LEN> {
public:
    using StrType = InplaceBuff<STATIC_LEN>;

    using StrType::StrType;
    using StrType::operator=;

    constexpr Blob(const Blob&) = default;
    constexpr Blob(Blob&&) noexcept = default;
    constexpr Blob& operator=(const Blob&) = default;
    constexpr Blob& operator=(Blob&&) noexcept = default;
    ~Blob() = default;
};

} // namespace sim::sql_fields
