#pragma once

#include <simlib/inplace_buff.hh>

namespace sim::sql_fields {

template <size_t MAX_LEN>
class Varbinary : public InplaceBuff<MAX_LEN + 1> { // +1 for a potential null byte
public:
    using StrType = InplaceBuff<MAX_LEN + 1>;
    static constexpr size_t max_len = MAX_LEN;

    using StrType::StrType;
    using StrType::operator=;

    constexpr Varbinary(const Varbinary&) = default;
    constexpr Varbinary(Varbinary&&) noexcept = default;
    constexpr Varbinary& operator=(const Varbinary&) = default;
    constexpr Varbinary& operator=(Varbinary&&) noexcept = default;
    ~Varbinary() = default;
};

template <class...>
constexpr inline bool is_varbinary = false;
template <size_t N>
constexpr inline bool is_varbinary<Varbinary<N>> = true;

} // namespace sim::sql_fields
