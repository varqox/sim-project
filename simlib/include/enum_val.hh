#pragma once

#include <type_traits>

template <class Enum>
class EnumVal {
public:
	static_assert(std::is_enum<Enum>::value,
	              "EnumVal is designed only for enums");
	using ValType = std::underlying_type_t<Enum>;

private:
	ValType val_;

public:
	EnumVal() = default;

	EnumVal(const EnumVal&) = default;
	EnumVal(EnumVal&&) = default;
	EnumVal& operator=(const EnumVal&) = default;
	EnumVal& operator=(EnumVal&&) = default;

	explicit EnumVal(ValType val) : val_(val) {}

	EnumVal(Enum val) : val_(static_cast<ValType>(val)) {}

	EnumVal& operator=(Enum val) {
		val_ = static_cast<ValType>(val);
		return *this;
	}

	operator Enum() const noexcept { return Enum(val_); }

	ValType int_val() const noexcept { return val_; }

	explicit operator ValType&() & noexcept { return val_; }

	explicit operator const ValType&() const& noexcept { return val_; }
};
