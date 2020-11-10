#pragma once

#include "simlib/debug.hh"

#include <semaphore.h>

namespace concurrent {

class Semaphore {
    sem_t sem{};

public:
    explicit Semaphore(unsigned value) {
        if (sem_init(&sem, 0, value)) {
            THROW("sem_init()", errmsg());
        }
    }

    Semaphore(const Semaphore&) = delete;
    Semaphore(Semaphore&&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore& operator=(Semaphore&&) = delete;

    void wait() {
        for (;;) {
            if (sem_wait(&sem) == 0) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }

            THROW("sem_wait()", errmsg());
        }
    }

    // Returns false iff the operation would block
    bool try_wait() {
        for (;;) {
            if (sem_trywait(&sem) == 0) {
                return true;
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN) {
                return false;
            }

            THROW("sem_trywait()", errmsg());
        }
    }

    void post() {
        if (sem_post(&sem)) {
            THROW("sem_post()", errmsg());
        }
    }

    ~Semaphore() { (void)sem_destroy(&sem); }
};

} // namespace concurrent
