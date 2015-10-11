#pragma once

#include <cstdio>
#include <sys/shm.h>
#include <sys/stat.h>

template<class T>
class UniquePtr {
	typedef T* pointer;

private:
	UniquePtr(const UniquePtr&);
	UniquePtr& operator=(const UniquePtr&);

	pointer p_;

public:
	explicit UniquePtr(pointer ptr = pointer()) : p_(ptr) {}

	~UniquePtr() {
		if (p_)
			delete p_;
	}

	void swap(UniquePtr& up) {
		pointer tmp = p_;
		p_ = up.p_;
		up.p_ = tmp;
	}

	pointer get() const { return p_; }

	pointer release(pointer ptr = pointer()) {
		pointer tmp = p_;
		p_ = ptr;
		return tmp;
	}

	void reset(pointer ptr = pointer()) {
		if (p_)
			delete p_;
		p_ = ptr;
	}

	T& operator*() const { return *p_; }

	pointer operator->() const { return p_; }

	T& operator[](size_t i) const { return p_[i]; }

	bool isNull() const { return p_ == pointer(); }
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
			addr_(nullptr) {

		if (id_ != -1) {
			if ((addr_ = shmat(id_, nullptr, 0)) == (void*)-1)
				addr_ = nullptr;
			shmctl(id_, IPC_RMID, nullptr);
		}
	}

	~SharedMemorySegment() {
		if (addr_)
			shmdt(addr_);
	}

	int key() const { return id_; }

	void* addr() const { return addr_; }
};
