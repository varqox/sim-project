#pragma once

#include <array>
#include <cstdint>
#include <simlib/throw_assert.hh>
#include <sys/socket.h>
#include <unistd.h>

/// For syncing between parent and child process in tests
class SyncSocket {
    std::array<int, 2> sock_fds;
    int* work_fd = nullptr;

public:
    SyncSocket() {
        throw_assert(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sock_fds.data()) == 0);
    }

    void i_am_parent_process() {
        work_fd = &sock_fds[0]; // NOLINT(readability-container-data-pointer)
        throw_assert(::close(sock_fds[1]) == 0);
        sock_fds[1] = -1;
    }

    void i_am_child_process() {
        work_fd = &sock_fds[1];
        throw_assert(::close(sock_fds[0]) == 0);
        sock_fds[0] = -1;
    }

    void send_byte() {
        uint8_t x = 0;
        throw_assert(write(*work_fd, &x, sizeof(x)) == sizeof(x));
    }

    void recv_byte() {
        uint8_t x = 0;
        throw_assert(read(*work_fd, &x, sizeof(x)) == sizeof(x));
    }

    void close() {
        throw_assert(::close(*work_fd) == 0);
        *work_fd = -1;
        work_fd = nullptr;
    }

    SyncSocket(const SyncSocket&) = delete;
    SyncSocket(SyncSocket&&) = delete;
    SyncSocket& operator=(const SyncSocket&) = delete;
    SyncSocket& operator=(SyncSocket&&) = delete;

    ~SyncSocket() noexcept(false) {
        for (auto fd : sock_fds) {
            if (fd != -1) {
                throw_assert(::close(fd) == 0);
            }
        }
    }
};
