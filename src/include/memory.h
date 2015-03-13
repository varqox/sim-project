#pragma once

template<class T>
class UniquePtr {
private:
	UniquePtr(const UniquePtr&);
	UniquePtr& operator=(const UniquePtr&);

	T* p_;

public:
	explicit UniquePtr(T* ptr = NULL): p_(ptr) {}

	~UniquePtr() {
		if (p_)
			delete p_;
	}

	void swap(UniquePtr<T>& up) { swap(p_, up.p_); }

	T* get() const { return p_; }

	T* release(T* ptr = NULL) {
		T* tmp = p_;
		p_ = ptr;
		return tmp;
	}

	void reset(T* ptr = NULL) {
		if (p_)
			delete p_;
		p_ = ptr;
	}

	T& operator*() const { return *p_; }

	T* operator->() const { return p_; }

	T& operator[](size_t i) const { return p_[i]; }

	bool isNull() const { return !p_; }
};

template<class A>
inline bool operator==(const UniquePtr<A>& x, A* y) { return x.get() == y; }

template<class A>
inline bool operator==(A* x, const UniquePtr<A>& y) { return x == y.get(); }

template<class A, class B>
inline bool operator==(const UniquePtr<A>& x, const UniquePtr<B>& y) {
	return x.get() == y.get();
}

template<class A>
inline bool operator!=(const UniquePtr<A>& x, A* y) { return x.get() != y; }

template<class A>
inline bool operator!=(A* x, const UniquePtr<A>& y) { return x != y.get(); }

template<class A, class B>
inline bool operator!=(const UniquePtr<A>& x, const UniquePtr<B>& y) {
	return x.get() != y.get();
}
