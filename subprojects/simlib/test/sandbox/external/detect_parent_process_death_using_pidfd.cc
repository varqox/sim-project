#include "check_poll.hh"
#include "sync_socket.hh"

#include <gtest/gtest.h>
#include <poll.h>
#include <simlib/syscalls.hh>
#include <simlib/throw_assert.hh>
#include <sys/types.h>
#include <sys/wait.h>

// NOLINTNEXTLINE
TEST(sandbox_external, detect_parent_process_death_using_pidfd) {
    SyncSocket sync_sock;
    pid_t child_pid = fork();
    ASSERT_NE(child_pid, -1);
    if (child_pid == 0) {
        sync_sock.i_am_child_process();
        int child_pidfd = syscalls::pidfd_open(getpid(), 0);
        throw_assert(child_pidfd != -1);
        pid_t grandchild_pid = fork();
        throw_assert(grandchild_pid != -1);
        if (grandchild_pid == 0) {
            // Pre-killing checks
            check_poll(child_pidfd, POLLIN, PollTimeout{});
            sync_sock.send_byte(); // signal parent to perform killing
            sync_sock.recv_byte(); // wait for parent to perform killing
            check_poll(child_pidfd, POLLIN, PollReady{POLLIN});
            sync_sock.send_byte(); // signal parent we are done
            _exit(0);
        }

        throw_assert(waitpid(grandchild_pid, nullptr, 0) == grandchild_pid);
        _exit(0);
    }
    sync_sock.i_am_parent_process();

    sync_sock.recv_byte(); // wait for grandchild to perform pre-killing checks
    ASSERT_EQ(kill(child_pid, SIGKILL), 0);
    sync_sock.send_byte(); // signal grandchild to perform post-killing checks
    sync_sock.recv_byte(); // wait for grandchild to perform post-killing checks

    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(child_pid, &status, 0), child_pid);
    ASSERT_TRUE(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
}
