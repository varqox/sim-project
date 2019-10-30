#pragma once

#include "inf_datetime.hh"
#include "varchar_field.hh"

namespace sim {

// Format: YYYY-mm-dd HH:MM:SS | # | @
class InfDatetimeField : public VarcharField<19> {
public:
	using VarcharField::VarcharField;

	constexpr InfDatetimeField(const InfDatetimeField&) = default;

	constexpr InfDatetimeField(InfDatetimeField&&) noexcept = default;

	using VarcharField::operator=;

	InfDatetimeField& operator=(const InfDatetimeField&) = default;

	InfDatetimeField& operator=(InfDatetimeField&&) noexcept = default;

	InfDatetime as_inf_datetime() { return InfDatetime(to_cstr()); }

	InfDatetime as_inf_datetime() const { return InfDatetime(*this); }

	operator InfDatetime() { return as_inf_datetime(); }

	operator InfDatetime() const { return as_inf_datetime(); }
};

} // namespace sim
