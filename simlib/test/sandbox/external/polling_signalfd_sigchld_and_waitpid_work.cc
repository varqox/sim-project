#include "check_poll.hh"
#include "sync_socket.hh"

#include <csignal>
#include <gtest/gtest.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox_external, signalfd_sigchld_waitpid) {
    sigset_t ss;
    ASSERT_EQ(sigemptyset(&ss), 0);
    ASSERT_EQ(sigaddset(&ss, SIGCHLD), 0);
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    ASSERT_EQ(sigprocmask(SIG_BLOCK, &ss, nullptr), 0);
    int sfd = signalfd(-1, &ss, SFD_CLOEXEC | SFD_NONBLOCK);
    ASSERT_NE(sfd, -1);

    SyncSocket sync_sock;
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        // Child
        sync_sock.i_am_child_process();
        sync_sock.recv_byte();
        _exit(0);
    }
    // Parent
    sync_sock.i_am_parent_process();
    check_poll(sfd, POLLIN, PollTimeout{});
    // Signal child to die
    sync_sock.send_byte();
    check_poll(sfd, POLLIN, PollReady{POLLIN});
    signalfd_siginfo ssi;
    ASSERT_EQ(read(sfd, &ssi, sizeof(ssi)), sizeof(ssi));
    ASSERT_EQ(ssi.ssi_signo, SIGCHLD);
    ASSERT_EQ(ssi.ssi_pid, pid);

    // Await child
    int status = 0;
    // Checks if waitpid() works while using signalfd on SIGCHLD
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
