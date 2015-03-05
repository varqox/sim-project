#pragma once

template<class T>
class UniquePtr {
private:
	T* p_;

public:
	UniquePtr(T* ptr = NULL): p_(ptr) {}

	~UniquePtr() {
		if (p_)
			delete p_;
	}

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

	T& operator[](size_t i) { return p_[i]; }
};
