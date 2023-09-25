#include "get_pid_of_the_only_child_process.hh"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <exception>
#include <gtest/gtest.h>
#include <simlib/random.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/string_traits.hh>
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
        auto sc = sandbox::spawn_supervisor();
        int supervisor_pid = get_pid_of_the_only_child_process();
        auto rh = sc.send_request({{"/bin/sleep", "10"}});
        // Kill the sandbox supervisor process randomly in time
        std::this_thread::sleep_for(std::chrono::nanoseconds{get_random(0, 10'000'000)});
        throw_assert(kill(supervisor_pid, SIGKILL) == 0);
        // Reap the sandbox supervisor process
        try {
            sc.await_result(std::move(rh));
            FAIL();
        } catch (const std::exception& e) {
            ASSERT_TRUE(has_prefix(
                e.what(), "sandbox supervisor died unexpectedly: killed by signal KILL - Killed"
            )) << e.what();
        }
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
