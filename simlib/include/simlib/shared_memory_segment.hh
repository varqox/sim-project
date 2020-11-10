#pragma once

#include "simlib/file_perms.hh"

#include <sys/shm.h>

class SharedMemorySegment {
private:
    int id_;
    void* addr_;

public:
    explicit SharedMemorySegment(size_t size)
    : id_(shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | S_0600))
    , addr_(nullptr) {
        if (id_ != -1) {
            if ((addr_ = shmat(id_, nullptr, 0)) == reinterpret_cast<void*>(-1)) {
                addr_ = nullptr;
            }
            shmctl(id_, IPC_RMID, nullptr);
        }
    }

    SharedMemorySegment(const SharedMemorySegment&) = delete;

    SharedMemorySegment(SharedMemorySegment&& sms) noexcept
    : id_(sms.id_)
    , addr_(sms.addr_) {
        sms.addr_ = nullptr;
    }

    SharedMemorySegment& operator=(const SharedMemorySegment&) = delete;

    SharedMemorySegment& operator=(SharedMemorySegment&& sms) noexcept {
        id_ = sms.id_;
        addr_ = sms.addr_;

        sms.addr_ = nullptr;
        return *this;
    }

    ~SharedMemorySegment() {
        if (addr_) {
            shmdt(addr_);
        }
    }

    [[nodiscard]] int key() const { return id_; }

    [[nodiscard]] void* addr() const { return addr_; }
};
