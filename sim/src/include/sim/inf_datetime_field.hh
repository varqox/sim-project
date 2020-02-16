#pragma once

#include "inf_datetime.hh"
#include "varchar_field.hh"

namespace sim {

// Format: YYYY-mm-dd HH:MM:SS | # | @
class InfDatetimeField : public VarcharField<19> {
public:
	InfDatetimeField() = default;

	constexpr InfDatetimeField(const InfDatetimeField&) = default;

	InfDatetimeField(InfDatetimeField&&) noexcept = default;

	InfDatetimeField(const InfDatetime& dt) : VarcharField(dt.to_str()) {}

	InfDatetimeField(InfDatetime&& dt) : VarcharField(dt.to_str()) {}

	template <class T,
	          std::enable_if_t<std::is_convertible_v<T, StringView>, int> = 0>
	InfDatetimeField(T&& str)
	   : InfDatetimeField(InfDatetime(std::forward<T>(str))) {}

	InfDatetimeField& operator=(const InfDatetimeField&) = default;

	InfDatetimeField& operator=(InfDatetimeField&&) noexcept = default;

	InfDatetimeField& operator=(const InfDatetime& dt) {
		VarcharField::operator=(dt.to_str());
		return *this;
	}

	InfDatetimeField& operator=(InfDatetime&& dt) {
		VarcharField::operator=(dt.to_str());
		return *this;
	}

	template <class T,
	          std::enable_if_t<std::is_convertible_v<T, StringView>, int> = 0>
	InfDatetimeField& operator=(T&& str) {
		return *this = InfDatetime(std::forward<T>(str));
	}

	InfDatetime as_inf_datetime() { return InfDatetime(to_cstr()); }

	InfDatetime as_inf_datetime() const { return InfDatetime(*this); }

	operator InfDatetime() { return as_inf_datetime(); }

	operator InfDatetime() const { return as_inf_datetime(); }
};

} // namespace sim
