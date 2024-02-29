#include "assert_result.hh"
#include "get_pid_of_the_only_child_process.hh"

#include <csignal>
#include <exception>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <simlib/concat_tostr.hh>
#include <simlib/pipe.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/string_traits.hh>
#include <simlib/syscalls.hh>
#include <simlib/throw_assert.hh>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, sandbox_reports_unexpected_supervisor_process_death_in_destructor) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        try {
            {
                auto sc = sandbox::spawn_supervisor();
                auto supervisor_pid = get_pid_of_the_only_child_process();
                throw_assert(kill(supervisor_pid, SIGTERM) == 0);
                // Wait for the supervisor process to die (if we don't, the SupervisorConnection may
                // see the supervisor process as still alive because SIGTERM is not yet processed
                // and will try to kill the supervisor itself and won't report the unexpected death)
                throw_assert(
                    syscalls::waitid(
                        P_PID, supervisor_pid, nullptr, __WALL | WEXITED | WNOWAIT, nullptr
                    ) == 0
                );
            }
            throw_assert(false && "expected exception to be thrown");
        } catch (const std::exception& e) {
            throw_assert(has_prefix(
                e.what(), "sandbox supervisor died unexpectedly: killed by signal TERM - Terminated"
            ));
        }
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_reports_unexpected_supervisor_process_death_in_send_request) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto sc = sandbox::spawn_supervisor();
        auto supervisor_pid = get_pid_of_the_only_child_process();
        throw_assert(kill(supervisor_pid, SIGTERM) == 0);
        // Wait for supervisor process to die (if we don't the SupervisorConnection may see the
        // supervisor process as still alive because SIGTERM is not yet processed and will try to
        // kill the supervisor itself and won't report the unexpected death)
        throw_assert(syscalls::waitid(P_ALL, 0, nullptr, __WALL | WEXITED | WNOWAIT, nullptr) == 0);
        try {
            (void)sc.send_request({{"/bin/true"}});
            throw_assert(false && "expected exception to be thrown");
        } catch (const std::exception& e) {
            throw_assert(has_prefix(
                e.what(), "sandbox supervisor died unexpectedly: killed by signal TERM - Terminated"
            ));
        }
        // Sending again should also throw
        try {
            (void)sc.send_request({{"/bin/true"}});
            throw_assert(false && "expected exception to be thrown");
        } catch (const std::exception& e) {
            throw_assert(has_prefix(e.what(), "sandbox supervisor is already dead"));
        }
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(sandbox, sandbox_reports_unexpected_supervisor_process_death_in_await_result) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto sc = sandbox::spawn_supervisor();
        auto supervisor_pid = get_pid_of_the_only_child_process();
        auto rh = sc.send_request({{"/bin/sleep", "infinity"}}
        ); // we need an unfinished request, because await_result() returns result of a completed
           // request without error
        throw_assert(kill(supervisor_pid, SIGTERM) == 0);
        // Wait for supervisor process to die (if we don't the SupervisorConnection may see the
        // supervisor process as still alive because SIGTERM is not yet processed and will try to
        // kill the supervisor itself and won't report the unexpected death)
        throw_assert(syscalls::waitid(P_ALL, 0, nullptr, __WALL | WEXITED | WNOWAIT, nullptr) == 0);
        try {
            sc.await_result(std::move(rh));
            throw_assert(false && "expected exception to be thrown");
        } catch (const std::exception& e) {
            throw_assert(has_prefix(
                e.what(), "sandbox supervisor died unexpectedly: killed by signal TERM - Terminated"
            ));
        }
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(
    sandbox,
    await_result_after_supervisor_death_returns_all_results_of_sucessful_requests_and_reports_unexpected_supervisor_death_in_await_result
) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto sc = sandbox::spawn_supervisor();
        auto supervisor_pid = get_pid_of_the_only_child_process();
        auto rh1 = sc.send_request({{"/bin/sh", "-c", "exit 42"}});
        auto rh2 = sc.send_request({{"/bin/sh", "-c", "exit 43"}});
        auto pipe = pipe2(O_CLOEXEC).value(); // NOLINT(bugprone-unchecked-optional-access)
        auto rh3 = sc.send_request({{"/usr/bin/printf", "x"}}, {.stdout_fd = pipe.writable});
        throw_assert(pipe.writable.close() == 0);
        // Wait for the last command to execute (it may not finish)
        char c;
        throw_assert(read_all(pipe.readable, &c, sizeof(c)) == 1);
        auto rh4 = sc.send_request({{"/bin/sleep", "infinity"}}
        ); // we need an unfinished request, because await_result() returns result of a completed
           // request without error
        throw_assert(kill(supervisor_pid, SIGTERM) == 0);
        // Wait for supervisor process to die (if we don't the SupervisorConnection may see the
        // supervisor process as still alive because SIGTERM is not yet processed and will try to
        // kill the supervisor itself and won't report the unexpected death)
        throw_assert(syscalls::waitid(P_ALL, 0, nullptr, __WALL | WEXITED | WNOWAIT, nullptr) == 0);
        bool completed = false;
        try {
            ASSERT_RESULT_OK(sc.await_result(std::move(rh1)), CLD_EXITED, 42);
            ASSERT_RESULT_OK(sc.await_result(std::move(rh2)), CLD_EXITED, 43);
            completed = true;
            sc.await_result(std::move(rh3)
            ); // it may not fail if the command completed before stoping the supervisor
            sc.await_result(std::move(rh4)); // this will always fail if the previous did not
            throw_assert(false && "expected exception to be thrown");
        } catch (const std::exception& e) {
            throw_assert(has_prefix(
                e.what(), "sandbox supervisor died unexpectedly: killed by signal TERM - Terminated"
            ));
        }
        throw_assert(completed);
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
