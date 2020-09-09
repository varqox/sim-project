#pragma once

#include <simlib/inplace_buff.hh>

namespace sim {

template <size_t MAX_LEN>
class VarcharField
: public InplaceBuff<MAX_LEN + 1> { // +1 for potential null byte
public:
	using StrType = InplaceBuff<MAX_LEN + 1>;
	static constexpr size_t max_len = MAX_LEN;

	using StrType::StrType;

	constexpr VarcharField(const VarcharField&) = default;

	constexpr VarcharField(VarcharField&&) noexcept = default;

	using StrType::operator=;

	constexpr VarcharField& operator=(const VarcharField&) = default;

	constexpr VarcharField& operator=(VarcharField&&) noexcept = default;
};

} // namespace sim
