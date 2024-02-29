#include "check_poll.hh"
#include "sync_socket.hh"

#include <cstdint>
#include <gtest/gtest.h>
#include <poll.h>
#include <simlib/syscalls.hh>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox_external, signalfd_sigchld_waitpid) {
    SyncSocket sync_sock;

    int pidfd;
    clone_args cl_args = {};
    cl_args.flags = CLONE_PIDFD;
    cl_args.pidfd = reinterpret_cast<uint64_t>(&pidfd);
    cl_args.exit_signal = SIGCHLD;
    auto pid = syscalls::clone3(&cl_args);
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        // Child
        sync_sock.i_am_child_process();
        sync_sock.recv_byte();
        _exit(0);
    }
    // Parent
    sync_sock.i_am_parent_process();
    check_poll(pidfd, POLLIN, PollTimeout{});
    // Signal child to die
    sync_sock.send_byte();
    check_poll(pidfd, POLLIN, PollReady{POLLIN});

    // Await child
    siginfo_t si;
    ASSERT_EQ(syscalls::waitid(P_PIDFD, pidfd, &si, WEXITED, nullptr), 0);
    ASSERT_EQ(si.si_code, CLD_EXITED);
    ASSERT_EQ(si.si_status, 0);

    // pidfd signals being readable after the process became waited
    check_poll(pidfd, POLLIN, PollReady{POLLIN});
}
