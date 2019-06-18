#pragma once

#include "filesystem.h"

#include <cstdlib>
#include <sys/shm.h>
#include <sys/stat.h>

// Deleter which use free to deallocate, useful for std::unique_ptr
template <class T>
struct delete_using_free {
	void operator()(T* p) const noexcept { free(p); }
};

class SharedMemorySegment {
private:
	int id_;
	void* addr_;

public:
	explicit SharedMemorySegment(size_t size)
	   : id_(shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_0600)),
	     addr_(nullptr) {
		if (id_ != -1) {
			if ((addr_ = shmat(id_, nullptr, 0)) == (void*)-1)
				addr_ = nullptr;
			shmctl(id_, IPC_RMID, nullptr);
		}
	}

	SharedMemorySegment(const SharedMemorySegment&) = delete;

	SharedMemorySegment(SharedMemorySegment&& sms)
	   : id_(sms.id_), addr_(sms.addr_) {
		sms.addr_ = nullptr;
	}

	SharedMemorySegment& operator=(const SharedMemorySegment&) = delete;

	SharedMemorySegment& operator=(SharedMemorySegment&& sms) {
		id_ = sms.id_;
		addr_ = sms.addr_;

		sms.addr_ = nullptr;
		return *this;
	}

	~SharedMemorySegment() {
		if (addr_)
			shmdt(addr_);
	}

	int key() const { return id_; }

	void* addr() const { return addr_; }
};
