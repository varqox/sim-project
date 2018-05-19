#pragma once

#include "string.h"

#include <algorithm>
#include <array>

template<class T, class... Args>
T& back_insert(T& reference, Args&&... args) {
	// Allocate more space (reserve is inefficient)
	size_t old_size = reference.size();
	reference.resize(old_size + sizeof...(args));
	reference.resize(old_size);

	(void)std::initializer_list<int>{
		(reference.emplace_back(std::forward<Args>(args)), 0)...
	};
	return reference;
}

template<size_t N, class... Args>
InplaceBuff<N>& back_insert(InplaceBuff<N>& ibuff, Args&&... args) {
	ibuff.append(std::forward<Args>(args)...);
	return ibuff;
}

template<class... Args>
std::string& back_insert(std::string& str, Args&&... args) {
	[&str](auto&&... xx) {
		size_t total_length = str.size();
		(void)std::initializer_list<int>{
			(total_length += string_length(xx), 0)...
		};

		// Allocate more space (reserve is inefficient)
		size_t old_size = str.size();
		str.resize(total_length);
		str.resize(old_size);

		(void)std::initializer_list<int>{
			(str += std::forward<decltype(xx)>(xx), 0)...
		};
	}(stringify(std::forward<Args>(args))...);

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
	explicit CallInDtor(Func&& f) try : func(f) {
	} catch (...) {
		f();
		throw;
	}

	explicit CallInDtor(const Func& f) try : func(f) {
	} catch (...) {
		f();
		throw;
	}

	CallInDtor(const CallInDtor&) = delete;
	CallInDtor& operator=(const CallInDtor&) = delete;

	CallInDtor(CallInDtor&& cid) : func(std::move(cid.func)) {
		cid.cancel();
	}

	CallInDtor& operator=(CallInDtor&& cid) {
		func = std::move(cid.func);
		cid.cancel();
		return *this;
	}

	bool active() const noexcept { return make_call; }

	void cancel() noexcept { make_call = false; }

	void restore() noexcept { make_call = true; }

	auto call_and_cancel() {
		make_call = false;
		return func();
	}

	~CallInDtor() {
		if (make_call)
			try { func(); } catch (...) {} // We cannot throw
	}
};

template<class Func>
inline CallInDtor<Func> make_call_in_destructor(Func func) {
	return CallInDtor<Func>{std::move(func)};
}

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

template<class A>
constexpr bool isOneOf(const A&) { return false; }

template<class A, class B, class... C>
constexpr bool isOneOf(const A& val, const B& first, const C&... others) {
	if (sizeof...(others) == 0)
		return (val == first);

	return (val == first or isOneOf(val, others...));
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
	InplaceArray() : size_(0), a_{}, p_(&a_[0]) {}

	explicit InplaceArray(size_t n)
		: size_(n), a_{}, p_(n > N ? new T[n] : &a_[0])
	{}

	InplaceArray(size_t n, const T& val)
		: size_(n), a_{}, p_(n > N ? new T[n] : &a_[0])
	{
		fill(begin(), end(), val);
	}

	template<size_t N1>
	InplaceArray(const InplaceArray<T, N1>& a) : InplaceArray(a.size_) {
		std::copy(a.p_, a.p_ + size_, p_);
	}

	template<size_t N1>
	InplaceArray(InplaceArray<T, N1>&& a) : size_(a.size_) {
		if (size_ <= N) {
			for (size_t i = 0; i < size_; ++i)
				(*this)[i] = std::move(a[i]);

		} else if (a.p_ != &a.a_[0]) { // move the pointer
			p_ = a.p_;
			a.p_ = &a.a_[0];
		} else { // allocate memory and then copy
			p_ = new T[size_];
			for (size_t i = 0; i < size_; ++i)
				(*this)[i] = std::move(a[i]);
		}

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
			for (size_t i = 0; i < size_; ++i)
				(*this)[i] = std::move(a[i]);
		} else if (size_ >= a.size_) {
			for (size_t i = 0; i < size_; ++i)
				(*this)[i] = std::move(a[i]);
		} else {
			deallocate();
			try {
				p_ = new T[a.size_];
			} catch (...) {
				p_ = &a_[0];
				throw;
			}
			for (size_t i = 0; i < size_; ++i)
				(*this)[i] = std::move(a[i]);
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
				max_size_ = N;
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
			size_t new_ms = meta::max(max_size_ << 1, n);
			T* new_p = new T[new_ms];
			std::move(p_, p_ + size_, new_p);
			deallocate();

			p_ = new_p;
			max_size_ = new_ms;
		}

		size_ = n;
	}

	template<class... Args>
	T& emplace_back(Args&&... args) {
		resize(size() + 1);
		return back() = T{std::forward<Args>(args)...};
	}

	void clear() noexcept { size_ = 0; }

	size_t size() const noexcept { return size_; }

	T* data() noexcept { return p_; }

	const T* data() const noexcept { return p_; }

	T* begin() noexcept { return data(); }

	T* end() noexcept { return data() + size(); }

	T& front() noexcept { return (*this)[0]; }

	const T& front() const noexcept { return (*this)[0]; }

	T& back() noexcept { return (*this)[size() - 1]; }

	const T& back() const noexcept { return (*this)[size() - 1]; }

	T& operator[](size_t i) noexcept { return p_[i]; }

	const T& operator[](size_t i) const noexcept { return p_[i]; }

	~InplaceArray() { deallocate(); }
};

template<class Func>
class shared_function {
	std::shared_ptr<Func> func_;

public:
	shared_function(Func&& func)
		: func_(std::make_shared<Func>(std::forward<Func>(func))) {}

	template<class... Args>
	auto operator()(Args&&... args) const {
		return (*func_)(std::forward<Args>(args)...);
	}
};

template<class Func>
auto make_shared_function(Func&& func) {
	return shared_function<Func>(std::forward<Func>(func));
}

#if __cplusplus > 201402L
#warning "Since C++17 inline constexpr variables and constexpr if will handle this"
#endif

template<class...>
struct is_pair : std::false_type {};

template<class A, class B>
struct is_pair<std::pair<A, B>> : std::true_type {};
