#pragma once

#include "string.h"

#include <algorithm>
#include <array>

template <class T, class... Args>
T& back_insert(T& reference, Args&&... args) {
	// Allocate more space (reserve is inefficient)
	size_t old_size = reference.size();
	reference.resize(old_size + sizeof...(args));
	reference.resize(old_size);

	(void)std::initializer_list<int> {
	   (reference.emplace_back(std::forward<Args>(args)), 0)...};
	return reference;
}

template <size_t N, class... Args>
InplaceBuff<N>& back_insert(InplaceBuff<N>& ibuff, Args&&... args) {
	ibuff.append(std::forward<Args>(args)...);
	return ibuff;
}

template <class... Args>
std::string& back_insert(std::string& str, Args&&... args) {
	[&str](auto&&... xx) {
		size_t total_length = str.size();
		(void)std::initializer_list<int> {
		   (total_length += string_length(xx), 0)...};

		// Allocate more space (reserve is inefficient)
		size_t old_size = str.size();
		str.resize(total_length);
		str.resize(old_size);

		(void)std::initializer_list<int> {
		   (str += std::forward<decltype(xx)>(xx), 0)...};
	}(stringify(std::forward<Args>(args))...);

	return str;
}

template <class T, class C>
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

template <class T, class C, class Comp>
constexpr typename T::const_iterator binaryFind(const T& x, const C& val,
                                                Comp&& comp) {
	auto beg = x.begin(), end = x.end();
	while (beg != end) {
		auto mid = beg + ((end - beg) >> 1);
		if (comp(*mid, val))
			beg = ++mid;
		else
			end = mid;
	}
	return (beg != x.end() && !comp(*beg, val) && !comp(val, *beg) ? beg
	                                                               : x.end());
}

template <class T, typename B, class C>
constexpr typename T::const_iterator
binaryFindBy(const T& x, B T::value_type::*field, const C& val) {
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

template <class T, typename B, class C, class Comp>
constexpr typename T::const_iterator
binaryFindBy(const T& x, B T::value_type::*field, const C& val, Comp&& comp) {
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
template <class A, class B>
inline bool binary_search(const A& a, B&& val) {
	return std::binary_search(a.begin(), a.end(), std::forward<B>(val));
}

template <class A, class B>
inline auto lower_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::lower_bound(a.begin(), a.end(), std::forward<B>(val));
}

template <class A, class B>
inline auto upper_bound(const A& a, B&& val) -> decltype(a.begin()) {
	return std::upper_bound(a.begin(), a.end(), std::forward<B>(val));
}

template <class A>
inline void sort(A& a) {
	std::sort(a.begin(), a.end());
}

template <class A, class Func>
inline void sort(A& a, Func&& func) {
	std::sort(a.begin(), a.end(), std::forward<Func>(func));
}

template <class Func>
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

	CallInDtor(CallInDtor&& cid) : func(std::move(cid.func)) { cid.cancel(); }

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
		if (make_call) {
			try {
				func();
			} catch (...) {
			} // We cannot throw
		}
	}
};

template <class A, class B>
constexpr bool isIn(const A& val, const B& sequence) {
	for (auto&& x : sequence)
		if (x == val)
			return true;
	return false;
}

template <class A, class B>
constexpr bool isIn(const A& val, const std::initializer_list<B>& sequence) {
	for (auto&& x : sequence)
		if (x == val)
			return true;
	return false;
}

template <class A>
constexpr bool isOneOf(const A&) {
	return false;
}

template <class A, class B, class... C>
constexpr bool isOneOf(const A& val, const B& first, const C&... others) {
	if (sizeof...(others) == 0)
		return (val == first);

	return (val == first or isOneOf(val, others...));
}

#if 0 // Clang does not support std::launder yet
#if __cplusplus > 201402L
#warning                                                                       \
   "Since C++17 std::launder() has to be used to wrap the reinterpret_casts below"
#endif
#endif

template <class T, size_t N>
class InplaceArray {
	size_t size_, max_size_;
	using Elem = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
	Elem a_[N];
	Elem* p_;

	void deallocate() noexcept {
		if (p_ != a_)
			delete[] p_;
	}

	void destruct_and_deallocate() noexcept {
		std::destroy(begin(), end());
		deallocate();
	}

public:
	InplaceArray() : size_(0), max_size_(N), p_(a_) {}

	explicit InplaceArray(size_t n)
	   : size_(n), max_size_(std::max(n, N)), p_(n > N ? new Elem[n] : a_) {
		std::uninitialized_default_construct(begin(), end());
	}

	InplaceArray(size_t n, const T& val)
	   : size_(n), max_size_(std::max(n, N)), p_(n > N ? new Elem[n] : a_) {
		std::uninitialized_fill(begin(), end(), val);
	}

	template <size_t N1>
	InplaceArray(const InplaceArray<T, N1>& a)
	   : size_(a.size_), max_size_(std::max(size_, N)),
	     p_(size_ > N ? new Elem[size_] : a_) {
		std::uninitialized_copy(a.begin(), a.end(), begin());
	}

	template <size_t N1>
	InplaceArray(InplaceArray<T, N1>&& a)
	   : size_(a.size_), max_size_(std::max(size_, N)) {
		if (size_ <= N) {
			std::uninitialized_move(a.begin(), a.end(), begin());

		} else if (a.p_ != a.a_) { // move the pointer
			p_ = a.p_;
			a.p_ = a.a_;
			a.max_size_ = N1;

		} else { // allocate memory and then move
			p_ = new Elem[size_];
			std::uninitialized_move(a.begin(), a.end(), begin());
		}

		a.size_ = 0;
	}

	template <size_t N1>
	InplaceArray& operator=(const InplaceArray<T, N1>& a) {
		if (a.size_ <= N) {
			destruct_and_deallocate();
			p_ = a_;
			max_size_ = N;
			try {
				std::uninitialized_copy(a.begin(), a.end(), begin());
				size_ = a.size_;
			} catch (...) {
				size_ = 0;
				throw;
			}

		} else if (a.size_ <= size_) {
			// At first, potentially free memory
			if (size_ > a.size_) {
				std::destroy(begin() + a.size_, end());
				size_ = a.size_;
			}

			// It is OK to use std::copy as the objects in p_[0..a.size_] exist
			std::copy(a.begin(), a.end(), begin());

		} else {
			destruct_and_deallocate();
			try {
				p_ = new Elem[a.size_];
				max_size_ = size_ = a.size_;
				std::uninitialized_copy(a.begin(), a.end(), begin());
			} catch (...) {
				p_ = a_;
				size_ = 0;
				max_size_ = N;
				throw;
			}
		}

		return *this;
	}

	template <size_t N1>
	InplaceArray& operator=(InplaceArray<T, N1>&& a) {
		if (a.size_ <= N) {
			destruct_and_deallocate();
			p_ = a_;
			max_size_ = N;
			try {
				std::uninitialized_move(a.begin(), a.end(), begin());
				size_ = a.size_;
			} catch (...) {
				size_ = 0;
				throw;
			}
		} else if (a.p_ != a.a_) {
			destruct_and_deallocate();
			p_ = a.p_;
			size_ = a.size_;
			max_size_ = a.max_size_;

			a.p_ = a.a_;
			a.max_size_ = N1;
		} else if (a.size_ <= size_) {
			// At first, potentially free memory
			if (size_ > a.size_) {
				std::destroy(begin() + a.size_, end());
				size_ = a.size_;
			}
			// It is OK to use std::move as the objects in p_[0..a.size_] exist
			std::move(a.begin(), a.end(), begin());
		} else {
			destruct_and_deallocate();
			try {
				p_ = new Elem[a.size_];
				std::uninitialized_move(a.begin(), a.end(), begin());
				max_size_ = size_ = a.size_;
			} catch (...) {
				p_ = a_;
				size_ = 0;
				max_size_ = N;
			}
		}

		a.size_ = 0;
		return *this;
	}

	/**
	 * @brief Increases array's max_size if needed, on reallcation all data is
	 *   lost
	 *
	 * @param n lower bound on new capacity of the array
	 */
	void lossy_reserve_for(size_t n) {
		if (n > max_size_) {
			size_t new_max_size = meta::max(max_size_ << 1, n);
			auto new_p = std::make_unique<Elem[]>(new_max_size);
			deallocate();
			p_ = new_p.release();
			size_ = 0;
			max_size_ = new_max_size;
		}
	}

	/**
	 * @brief Increases array's max_size if needed, preserves data
	 *
	 * @param n lower bound on new capacity of the array
	 */
	void reserve_for(size_t n) {
		if (n > max_size_) {
			size_t new_max_size = meta::max(max_size_ << 1, n);
			auto new_p = std::make_unique<Elem[]>(new_max_size);
			std::uninitialized_move(iterator(new_p.get()),
			                        iterator(new_p.get() + size_), begin());
			deallocate();
			p_ = new_p.release();
			max_size_ = new_max_size;
		}
	}

	/**
	 * @brief Changes array's size and max_size if needed, preserves data
	 *
	 * @param n new array's size
	 */
	void resize(size_t n) {
		reserve_for(n);
		std::uninitialized_default_construct(begin(), begin() + n);
		size_ = n;
	}

	/**
	 * @brief Changes array's size and max_size if needed, preserves data
	 *
	 * @param n new array's size
	 * @param val the value to which the new elements will be set
	 */
	void resize(size_t n, const T& val) {
		reserve_for(n);

		if (n < size_)
			std::destroy(begin() + n, end());
		else
			std::uninitialized_fill(end(), begin() + n, val);
	}

	template <class... Args>
	T& emplace_back(Args&&... args) {
		reserve_for(size_ + 1);
		return *(::new (std::addressof((*this)[size_++]))
		            T(std::forward<Args>(args)...));
	}

	void clear() noexcept { size_ = 0; }

	size_t size() const noexcept { return size_; }

private:
	template <class Type>
	class Iterator {
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = Type;
		using difference_type = std::ptrdiff_t;
		using pointer = Type*;
		using reference = Type&;

	private:
		Elem* p = nullptr;

		Iterator(Elem* x) : p(x) {}

		friend class InplaceArray;

	public:
		Iterator() = default;

		reference operator*() const noexcept {
			return *reinterpret_cast<T*>(p);
		}

		pointer operator->() const noexcept { return reinterpret_cast<T*>(p); }

		Iterator& operator++() noexcept {
			++p;
			return *this;
		}

		Iterator operator++(int) noexcept { return Iterator(p++); }

		Iterator& operator--() noexcept {
			--p;
			return *this;
		}

		Iterator operator--(int) noexcept { return Iterator(p++); }

		reference operator[](difference_type n) const noexcept {
			return *reinterpret_cast<T*>(p + n);
		}

		Iterator& operator+=(difference_type n) const noexcept {
			p += n;
			return *this;
		}

		Iterator operator+(difference_type n) const noexcept {
			return Iterator(p + n);
		}

		Iterator& operator-=(difference_type n) const noexcept {
			p -= n;
			return *this;
		}

		Iterator operator-(difference_type n) const noexcept {
			return Iterator(p - n);
		}

		friend bool operator==(Iterator a, Iterator b) noexcept {
			return (a.p == b.p);
		}

		friend bool operator!=(Iterator a, Iterator b) noexcept {
			return (a.p != b.p);
		}

		friend bool operator<(Iterator a, Iterator b) noexcept {
			return (a.p < b.p);
		}

		friend bool operator>(Iterator a, Iterator b) noexcept {
			return (a.p > b.p);
		}

		friend bool operator<=(Iterator a, Iterator b) noexcept {
			return (a.p <= b.p);
		}

		friend bool operator>=(Iterator a, Iterator b) noexcept {
			return (a.p >= b.p);
		}

		friend difference_type operator-(Iterator a, Iterator b) noexcept {
			return (a.p - b.p);
		}

		friend Iterator operator+(difference_type n, Iterator b) noexcept {
			return Iterator(n + b.p);
		}
	};

public:
	using iterator = Iterator<T>;
	using const_iterator = Iterator<const T>;

	static_assert(sizeof(Elem) == sizeof(T), "Needed by data()");
	T* data() noexcept { return std::addressof(front()); }

	const T* data() const noexcept { return std::addressof(front()); }

	iterator begin() noexcept { return iterator(p_); }

	const_iterator begin() const noexcept { return const_iterator(p_); }

	iterator end() noexcept { return iterator(p_ + size_); }

	const_iterator end() const noexcept { return const_iterator(p_ + size_); }

	T& front() noexcept { return begin()[0]; }

	const T& front() const noexcept { return begin()[0]; }

	T& back() noexcept { return end()[-1]; }

	const T& back() const noexcept { return end()[-1]; }

	T& operator[](size_t i) noexcept { return begin()[i]; }

	const T& operator[](size_t i) const noexcept { return begin()[i]; }

	~InplaceArray() { destruct_and_deallocate(); }
};

template <class Func>
class shared_function {
	std::shared_ptr<Func> func_;

public:
	shared_function(Func&& func)
	   : func_(std::make_shared<Func>(std::forward<Func>(func))) {}

	template <class... Args>
	auto operator()(Args&&... args) const {
		return (*func_)(std::forward<Args>(args)...);
	}
};

template <class Func>
auto make_shared_function(Func&& func) {
	return shared_function<Func>(std::forward<Func>(func));
}

template <class...>
constexpr bool is_pair = false;

template <class A, class B>
constexpr bool is_pair<std::pair<A, B>> = true;

template <class Enum>
class EnumVal {
public:
	static_assert(std::is_enum<Enum>::value, "This is designed only for enums");
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

	explicit operator ValType() const noexcept { return val_; }

	explicit operator ValType&() & noexcept { return val_; }
};
