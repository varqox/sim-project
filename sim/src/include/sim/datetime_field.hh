#pragma once

#include "varchar_field.hh"

namespace sim {

// Format: YYYY-mm-dd HH:MM:SS
class DatetimeField : public VarcharField<19> {
public:
	DatetimeField() = default;

	constexpr DatetimeField(const DatetimeField&) = default;

	constexpr DatetimeField(DatetimeField&&) noexcept = default;

	template <class T,
	          std::enable_if_t<std::is_convertible_v<T, StringView>, int> = 0>
	constexpr DatetimeField(T&& str)
	   : VarcharField([&]() -> decltype(auto) {
		     throw_assert(
		        isDatetime(intentionalUnsafeCStringView(concat(str))));
		     return std::forward<T>(str);
	     }()) {}

	DatetimeField& operator=(const DatetimeField&) = default;

	DatetimeField& operator=(DatetimeField&&) noexcept = default;

	template <class T,
	          std::enable_if_t<std::is_convertible_v<T, StringView>, int> = 0>
	DatetimeField& operator=(T&& str) {
		throw_assert(isDatetime(intentionalUnsafeCStringView(concat(str))));
		VarcharField::operator=(std::forward<T>(str));
		return *this;
	}
};

} // namespace sim
