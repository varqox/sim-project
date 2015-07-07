#pragma once

#include <cstdio>
#include <sys/shm.h>
#include <sys/stat.h>

template<class T>
class UniquePtr {
private:
	UniquePtr(const UniquePtr&);
	UniquePtr& operator=(const UniquePtr&);

	T* p_;

public:
	explicit UniquePtr(T* ptr = NULL) : p_(ptr) {}

	~UniquePtr() {
		if (p_)
			delete p_;
	}

	void swap(UniquePtr<T>& up) {
		T* tmp = p_;
		p_ = up.p_;
		up.p_ = tmp;
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

class SharedMemorySegment {
private:
	int id_;
	void* addr_;
	SharedMemorySegment(const SharedMemorySegment&);
	SharedMemorySegment& operator=(const SharedMemorySegment&);

public:
	SharedMemorySegment(size_t size) : id_(shmget(IPC_PRIVATE, size,
				IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)),
			addr_(NULL) {

		if (id_ != -1) {
			if ((addr_ = shmat(id_, NULL, 0)) == (void*)-1)
				addr_ = NULL;
			shmctl(id_, IPC_RMID, NULL);
		}
	}

	~SharedMemorySegment() {
		if (addr_)
			shmdt(addr_);
	}

	int key() const { return id_; }

	void* addr() const { return addr_; }
};
