#include <cerrno>
#include <chrono>
#include <csignal>
#include <gtest/gtest.h>
#include <simlib/random.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/throw_assert.hh>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_terminates_early_upon_client_process_death) {
    // Do test in the child process to prevent adoption of the other child processes
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        throw_assert(prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) == 0);
        pid_t sandbox_client_pid = fork();
        throw_assert(sandbox_client_pid != -1);
        if (sandbox_client_pid == 0) {
            auto sc = sandbox::spawn_supervisor();
            sc.send_request({{"/bin/sleep", "10"}});
            sc.await_result();
            _exit(0);
        }
        // Kill the sandbox client process randomly in time
        std::this_thread::sleep_for(std::chrono::nanoseconds{get_random(0, 10'000'000)});
        throw_assert(kill(sandbox_client_pid, SIGKILL) == 0);
        // Reap the sandbox client process
        int status = 0;
        throw_assert(waitpid(sandbox_client_pid, &status, 0) == sandbox_client_pid);
        throw_assert(WIFSIGNALED(status));
        throw_assert(WTERMSIG(status) == SIGKILL);
        // Reap all other children
        auto reaping_start = std::chrono::steady_clock::now();
        for (;;) {
            pid_t child_pid = wait(nullptr);
            if (child_pid == -1) {
                throw_assert(errno == ECHILD);
                break;
            }
        }
        auto reaping_took = std::chrono::steady_clock::now() - reaping_start;
        throw_assert(reaping_took < std::chrono::milliseconds{800});
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
