#pragma once

#include "varchar_field.hh"

namespace sim {

// Format: YYYY-mm-dd HH:MM:SS
class DatetimeField : public VarcharField<19> {
public:
	using VarcharField::VarcharField;

	constexpr DatetimeField(const DatetimeField&) = default;

	constexpr DatetimeField(DatetimeField&&) noexcept = default;

	using VarcharField::operator=;

	DatetimeField& operator=(const DatetimeField&) = default;

	DatetimeField& operator=(DatetimeField&&) noexcept = default;
};

} // namespace sim
