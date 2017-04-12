#pragma once

#include "meta.h"

#include <algorithm>
#include <array>

template<class T, class... Args>
T& back_insert(T& reference, Args&&... args) {
	reference.reserve(reference.size() + sizeof...(args));
	int t[] = {(reference.emplace_back(std::forward<Args>(args)), 0)...};
	(void)t;
	return reference;
}

template<class... Args>
std::string& back_insert(std::string& str, Args&&... args) {
	size_t total_length = str.size();
	int foo[] = {(total_length += string_length(args), 0)...};
	(void)foo;

	str.reserve(total_length);
	int bar[] = {(str += std::forward<Args>(args), 0)...};
	(void)bar;
	return str;
}

template<class T, class C>
constexpr typename T::const_iterator binaryFind(const T& x, const C& val) {
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (*mid < val)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && *beg == val ? beg : x.end());
}

template<class T, class C, class Comp>
constexpr typename T::const_iterator binaryFind(const T& x, const C& val,
	Comp&& comp)
{
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (comp(*mid, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && !comp(*beg, val) && !comp(val, *beg) ?
		beg : x.end());
}

template<class T, typename B, class C>
constexpr typename T::const_iterator binaryFindBy(const T& x,
	B T::value_type::*field, const C& val)
{
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if ((*mid).*field < val)
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}

template<class T, typename B, class C, class Comp>
constexpr typename T::const_iterator binaryFindBy(const T& x,
	B T::value_type::*field, const C& val, Comp&& comp)
{
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (comp((*mid).*field, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && (*beg).*field == val ? beg : x.end());
}

// Aliases - take container instead of range
template<class A, class B>
inline bool binary_search(const A& a, B&& val) {
	return std::binary_search(a.begin(), a.end(), std::forward<B>(val));
}

template<class A, class B>
inline auto lower_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::lower_bound(a.begin(), a.end(), std::forward<B>(val));
}

template<class A, class B>
inline auto upper_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::upper_bound(a.begin(), a.end(), std::forward<B>(val));
}

template<class A>
inline void sort(A& a) { std::sort(a.begin(), a.end()); }

template<class A, class Func>
inline void sort(A& a, Func&& func) {
	std::sort(a.begin(), a.end(), std::forward<Func>(func));
}

template<class Func>
class CallInDtor {
	Func func;
	bool make_call = true;

public:
	constexpr explicit CallInDtor(Func f) : func(std::move(f)) {}

	CallInDtor(const CallInDtor&) = delete;
	CallInDtor(CallInDtor&&) = delete;
	CallInDtor& operator=(const CallInDtor&) = delete;
	CallInDtor& operator=(CallInDtor&&) = delete;

	void cancel() { make_call = false; }

	void restore() { make_call = true; }

	auto call_and_cancel() {
		make_call = false;
		return func();
	}

	~CallInDtor() {
		if (make_call)
			try { func(); } catch (...) {} // We cannot throw
	}
};

template<class A, class B>
constexpr bool isIn(const A& val, const B& sequence) {
	for (auto&& x : sequence)
		if (x == val)
			return true;
	return false;
}

template<class A, class B>
constexpr bool isIn(const A& val, const std::initializer_list<B>& sequence) {
	for (auto&& x : sequence)
		if (x == val)
			return true;
	return false;
}

template<class T, size_t N>
class InplaceArray {
	size_t size_, max_size_;
	std::array<T, N> a_;
	T* p_;

	void deallocate() noexcept {
		if (p_ != &a_[0])
			delete[] p_;
	}

public:
	explicit InplaceArray(size_t n)
		: size_(n), p_(n > N ? new T[n] : &a_[0]) {}

	InplaceArray(size_t n, const T& val)
		: size_(n), p_(n > N ? new T[n] : &a_[0])
	{
		fill(p_, p_ + n, val);
	}

	template<size_t N1>
	InplaceArray(const InplaceArray<T, N1>& a) : InplaceArray(a.size_) {
		copy(a.p_, a.p_ + size_, p_);
	}

	template<size_t N1>
	InplaceArray(InplaceArray<T, N1>&& a) : size_(a.size_) {
		if (size_ <= N) // copy to the array
			std::copy(a.p_, a.p_ + size_, p_ = &a[0]);
		else if (a.p_ != &a.a_[0]) { // move the pointer
			p_ = a.p_;
			a.p_ = &a.a_[0];
		} else // allocate memory and then copy
			std::copy(a.p_, a.p_ + size_, p_ = new T[size_]);

		a.size_ = 0;
	}

	template<size_t N1>
	InplaceArray& operator=(const InplaceArray<T, N1>& a) {
		if (a.size_ <= N) {
			deallocate();
			p_ = &a_[0];
			std::copy(a.p_, a.p_ + a.size_, p_);
		} else if (size_ >= a.size_) {
			std::copy(a.p_, a.p_ + a.size_, p_);
		} else {
			deallocate();
			try {
				p_ = new T[a.size_];
			} catch (...) {
				p_ = &a_[0];
				throw;
			}
			std::copy(a.p_, a.p_ + a.size_, p_);
		}

		size_ = a.size_;
		return *this;
	}

	template<size_t N1>
	InplaceArray& operator=(InplaceArray<T, N1>&& a) {
		if (a.p_ != &a.a_[0]) {
			deallocate();
			p_ = a.p_;
			a.p_ = &a.a_[0];
		} else if (a.size_ <= N) {
			deallocate();
			p_ = &a_[0];
			std::copy(a.p_, a.p_ + a.size_, p_);
		} else if (size_ >= a.size_) {
			std::copy(a.p_, a.p_ + a.size_, p_);
		} else {
			deallocate();
			try {
				p_ = new T[a.size_];
			} catch (...) {
				p_ = &a_[0];
				throw;
			}
			std::copy(a.p_, a.p_ + a.size_, p_);
		}

		size_ = a.size_;
		a.size_ = 0;
		return *this;
	}

	/**
	 * @brief Changes array's size and max_size if needed, but in the latter
	 *   case all the data are lost
	 *
	 * @param n new array's size
	 */
	void lossy_resize(size_t n) {
		if (n > max_size_) {
			max_size_ = meta::max(max_size_ << 1, n);
			deallocate();
			try {
				p_ = new T[max_size_];
			} catch (...) {
				p_ = &a_[0];
				throw;
			}
		}

		size_ = n;
	}

	/**
	 * @brief Changes array's size and max_size if needed, preserves data
	 *
	 * @param n new array's size
	 */
	void resize(size_t n) {
		if (n > max_size_) {
			max_size_ = meta::max(max_size_ << 1, n);
			T* new_p = new T[max_size_];
			std::copy(p_, p_ + size_, new_p);
			deallocate();
			p_ = new_p;
		}

		size_ = n;
	}

	size_t size() const noexcept { return size_; }

	T* data() noexcept { return p_; }

	const T* data() const noexcept { return p_; }

	T& operator[](size_t i) noexcept { return p_[i]; }

	const T& operator[](size_t i) const noexcept { return p_[i]; }

	~InplaceArray() { deallocate(); }
};
