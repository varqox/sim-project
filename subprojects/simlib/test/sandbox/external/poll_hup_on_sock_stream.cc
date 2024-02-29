#include "check_poll.hh"
#include "sync_socket.hh"

#include <cstdlib>
#include <gtest/gtest.h>
#include <poll.h>
#include <simlib/throw_assert.hh>
#include <unistd.h>

enum class PendingData {
    NONE,
    SOME,
};

enum class Order {
    SHUTDOWN_RD_THEN_WR,
    SHUTDOWN_WR_THEN_RD,
};

template <class F1, class F2, class F3, class F4>
void test(
    PendingData pending_data,
    Order order,
    F1 before_shutdown,
    F2 after_first_shutdown,
    F3 after_second_shutdown,
    F4 after_close
) {
    int sfds[2];
    auto& parent_sfd = sfds[0];
    auto& child_sfd = sfds[1];
    throw_assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sfds) == 0);
    switch (pending_data) {
    case PendingData::NONE: break;
    case PendingData::SOME: {
        throw_assert(write(parent_sfd, "x", 1) == 1);
    } break;
    }

    SyncSocket sync_sock;
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        throw_assert(close(parent_sfd) == 0);
        sync_sock.i_am_child_process();

        before_shutdown(child_sfd);
        sync_sock.send_byte(); // signal parent to perform shutdown
        sync_sock.recv_byte(); // wait for parent to perform shutdown
        after_first_shutdown(child_sfd);
        sync_sock.send_byte(); // signal parent to perform shutdown
        sync_sock.recv_byte(); // wait for parent to perform shutdown
        after_second_shutdown(child_sfd);
        sync_sock.send_byte(); // signal parent to perform close
        sync_sock.recv_byte(); // wait for parent to perform close
        after_close(child_sfd);

        _exit(0);
    }
    // Parent
    throw_assert(close(child_sfd) == 0);
    sync_sock.i_am_parent_process();
    // First shutdown
    sync_sock.recv_byte(); // wait for child to perform before_shutdown
    switch (order) {
    case Order::SHUTDOWN_RD_THEN_WR: {
        throw_assert(shutdown(parent_sfd, SHUT_RD) == 0);
    } break;
    case Order::SHUTDOWN_WR_THEN_RD: {
        throw_assert(shutdown(parent_sfd, SHUT_WR) == 0);
    } break;
    }
    sync_sock.send_byte(); // signal child that shutdown were performed
    // Second shutdown
    sync_sock.recv_byte(); // wait for child to perform after_first_shutdown
    switch (order) {
    case Order::SHUTDOWN_RD_THEN_WR: {
        throw_assert(shutdown(parent_sfd, SHUT_WR) == 0);
    } break;
    case Order::SHUTDOWN_WR_THEN_RD: {
        throw_assert(shutdown(parent_sfd, SHUT_RD) == 0);
    } break;
    }
    sync_sock.send_byte(); // signal child that shutdown were performed
    // Close
    sync_sock.recv_byte(); // wait for child to perform after_second_shutdown
    throw_assert(close(parent_sfd) == 0);
    sync_sock.send_byte(); // signal child that shutdown were performed
    // Await child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    // Clean up
    sync_sock.close();
}

// NOLINTNEXTLINE
TEST(sandbox_external, poll_hup_no_data_shutdown_rd_wr) {
    test(
        PendingData::NONE,
        Order::SHUTDOWN_RD_THEN_WR,
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollTimeout{});
            check_poll(fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLOUT});
        },
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollTimeout{});
            check_poll(fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLOUT});
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        }
    );
}

// NOLINTNEXTLINE
TEST(sandbox_external, poll_hup_no_data_shutdown_wr_rd) {
    test(
        PendingData::NONE,
        Order::SHUTDOWN_WR_THEN_RD,
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollTimeout{});
            check_poll(fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLOUT});
        },
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP});
            check_poll(
                fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLIN | POLLOUT | POLLRDHUP}
            );
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        }
    );
}

// NOLINTNEXTLINE
TEST(sandbox_external, poll_hup_with_pending_data_shutdown_rd_wr) {
    test(
        PendingData::SOME,
        Order::SHUTDOWN_RD_THEN_WR,
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollTimeout{});
            check_poll(fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLIN | POLLOUT});
        },
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollTimeout{});
            check_poll(fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLIN | POLLOUT});
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        }
    );
}

// NOLINTNEXTLINE
TEST(sandbox_external, poll_hup_with_pending_data_shutdown_wr_rd) {
    test(
        PendingData::SOME,
        Order::SHUTDOWN_WR_THEN_RD,
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollTimeout{});
            check_poll(fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLIN | POLLOUT});
        },
        [](int fd) {
            check_poll(fd, 0, PollTimeout{});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP});
            check_poll(
                fd, POLLIN | POLLOUT | POLLPRI | POLLRDHUP, PollReady{POLLIN | POLLOUT | POLLRDHUP}
            );
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        },
        [](int fd) {
            check_poll(fd, 0, PollReady{POLLHUP});
            check_poll(fd, POLLRDHUP, PollReady{POLLRDHUP | POLLHUP});
            check_poll(
                fd,
                POLLIN | POLLOUT | POLLPRI | POLLRDHUP,
                PollReady{POLLIN | POLLOUT | POLLRDHUP | POLLHUP}
            );
        }
    );
}
