#pragma once

#include "debug.h"

#if __cplusplus > 201402L
#warning "Since C++17 this is redundant"
#include <optional>
#else
namespace std {
struct nullopt_t {
	explicit constexpr nullopt_t(int) {}
};
constexpr nullopt_t nullopt{1};
}
#endif

template<class T, bool>
class OptionalBase;

template<class T>
class OptionalBase<T, true> {
protected:
	using StoredType = std::remove_const_t<T>;
	struct EmptyByte {};
	union {
		EmptyByte empty_;
		StoredType payload_;
	};
	bool has_value_;

	constexpr OptionalBase(bool has_value) noexcept : has_value_(has_value) {}
};

template<class T>
class OptionalBase<T, false> {
protected:
	using StoredType = std::remove_const_t<T>;
	struct EmptyByte {};
	union {
		EmptyByte empty_;
		StoredType payload_;
	};
	bool has_value_;

	constexpr OptionalBase(bool has_value) noexcept : has_value_(has_value) {}

	~OptionalBase() {
		if (has_value_)
			payload_.~StoredType();
	}
};

template<class T>
class Optional : private OptionalBase<T, std::is_trivially_destructible<T>::value> {
	using Base = OptionalBase<T, std::is_trivially_destructible<T>::value>;
	using StoredType = typename Base::StoredType;
	using Base::payload_;
	using Base::has_value_;

	template<class U>
	friend class Optional;

public:
	constexpr Optional() noexcept : Base(false) {}

	constexpr Optional(std::nullopt_t) noexcept : Optional() {}

	template<class U = T, class X = std::enable_if_t<std::is_constructible<StoredType, U&&>::value>>
	constexpr Optional(U&& value) : Base(true) {
		::new (std::addressof(payload_)) StoredType(value);
	}

	constexpr Optional(const Optional& other) : Base(other.has_value_) {
		if (other.has_value_)
			::new(std::addressof(payload_)) StoredType(other.payload_);
	}

	constexpr Optional(Optional&& other) : Base(other.has_value_) {
		if (other.has_value_) {
			::new(std::addressof(payload_)) StoredType(std::move(other.payload_));
			other.has_value_ = false;
		}
	}

	template<class U>
	constexpr Optional(const Optional<U>& other) : Base(other.has_value_) {
		if (other.has_value_)
			::new(std::addressof(payload_)) StoredType(other.payload_);
	}

	template<class U>
	constexpr Optional(Optional<U>&& other) : Base(other.has_value_) {
		if (other.has_value_) {
			::new(std::addressof(payload_)) StoredType(std::move(other.payload_));
			other.has_value_ = false;
		}
	}

	constexpr Optional& operator=(std::nullopt_t) noexcept {
		if (has_value_) {
			has_value_ = false;
			payload_.~StoredType();
		}
		return *this;
	}

	constexpr Optional& operator=(const Optional& other) {
		return operator=<T>(other);
	}

	constexpr Optional& operator=(Optional&& other) {
		return operator=<T>(other);
	}

	template<class U>
	constexpr Optional& operator=(const Optional<U>& other) {
		if (has_value_) {
			payload_.~StoredType();
			has_value_ = false;
		}

		if (other.has_value_) {
			::new(std::addressof(payload_)) StoredType(other.payload_);
			has_value_ = true;
		}

		return *this;
	}

	template<class U>
	constexpr Optional& operator=(Optional<U>&& other) {
		if (has_value_) {
			payload_.~StoredType();
			has_value_ = false;
		}

		if (other.has_value_) {
			::new(std::addressof(payload_)) StoredType(std::move(other.payload_));
			other.has_value_ = false;
			has_value_ = true;
		}

		return *this;
	}

	template<class U = T, class X = std::enable_if_t<std::is_constructible<StoredType, U&&>::value>>
	constexpr Optional& operator=(U&& value) {
		if (has_value_) {
			payload_.~StoredType();
			has_value_ = false;
		}

		::new(std::addressof(payload_)) StoredType(std::forward<U>(value));
		has_value_ = true;
		return *this;
	}

	constexpr explicit operator bool() const noexcept { return has_value_; }

	constexpr T& operator*() { return payload_; }

	constexpr const T& operator*() const { return payload_; }

	constexpr T* operator->() { return std::addressof(payload_); }

	constexpr const T* operator->() const { return std::addressof(payload_); }

	constexpr bool has_value() const noexcept { return has_value_; }

	constexpr T& value() {
		if (not has_value_)
			THROW("bad optional access");

		return payload_;
	}

	constexpr const T& value() const {
		if (not has_value_)
			THROW("bad optional access");

		return payload_;
	}

	template<class U>
	constexpr T value_or(U&& default_value) const {
		return (has_value_ ? payload_ :
			static_cast<T>(std::forward<U>(default_value)));
	}
};
