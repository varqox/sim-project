#include "debug.h"

template<class T>
class Optional {
	bool has_value_;
	T val_;

public:
	Optional() : has_value_(false) {}

	Optional(std::nullptr_t) : Optional() {}

	Optional(const T& value) : has_value_(true), val_(value) {}

	Optional(T&& value) : has_value_(true), val_(value) {}

	Optional(const Optional&) = default;
	Optional(Optional&&) = default;
	Optional& operator=(const Optional&) = default;
	Optional& operator=(Optional&&) = default;

	Optional& operator=(std::nullptr_t) noexcept {
		has_value_ = false;
		return *this;
	}

	template<class U = T>
	Optional& operator=(U&& value) {
		has_value_ = true;
		val_ = std::forward<U>(value);
		return *this;
	}

	template<class U>
	Optional& operator=(Optional<U>& value) {
		has_value_ = value.has_value_;
		val_ = value.val_;
		return *this;
	}

	template<class U>
	Optional& operator=(const Optional<U>& value) {
		has_value_ = value.has_value_;
		val_ = value.val_;
		return *this;
	}

	template<class U>
	Optional& operator=(Optional<U>&& value) {
		has_value_ = value.has_value_;
		value.has_value_ = false;
		val_ = std::move(value.val_);
		return *this;
	}

	constexpr explicit operator bool() const noexcept { return has_value_; }

	constexpr bool has_value() const noexcept { return has_value_; }

	constexpr T& value() {
		if (not has_value_)
			THROW("bad optional access");

		return val_;
	}

	constexpr const T& value() const {
		if (not has_value_)
			THROW("bad optional access");

		return val_;
	}

	template<class U>
	constexpr T value_or(U&& default_value) const {
		return (has_value_ ? val_ : std::forward<U>(default_value));
	}
};
