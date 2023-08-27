#include <cctype>
#include <csignal>
#include <exception>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <simlib/concat_tostr.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_perms.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/syscalls.hh>
#include <simlib/temporary_directory.hh>
#include <simlib/throw_assert.hh>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t get_pid_of_the_only_child_process() {
    auto children_pids = get_file_contents(concat_tostr("/proc/self/task/", gettid(), "/children"));
    auto child_pid = str2num<pid_t>(StringView{children_pids}.without_trailing(&isspace));
    throw_assert(child_pid.has_value());
    return *child_pid;
}

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
            sc.send_request("");
            throw_assert(false && "expected exception to be thrown");
        } catch (const std::exception& e) {
            throw_assert(has_prefix(
                e.what(), "sandbox supervisor died unexpectedly: killed by signal TERM - Terminated"
            ));
        }
        // Sending again should also throw
        try {
            sc.send_request("");
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
        sc.send_request("sleep infinity"); // we need an unfinished request, because await_result()
                                           // returns result of a completed request without error
        throw_assert(kill(supervisor_pid, SIGTERM) == 0);
        // Wait for supervisor process to die (if we don't the SupervisorConnection may see the
        // supervisor process as still alive because SIGTERM is not yet processed and will try to
        // kill the supervisor itself and won't report the unexpected death)
        throw_assert(syscalls::waitid(P_ALL, 0, nullptr, __WALL | WEXITED | WNOWAIT, nullptr) == 0);
        try {
            sc.await_result();
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
        sc.send_request("exit 42");
        sc.send_request("exit 43");
        {
            TemporaryDirectory tmp_dir{"/tmp/sandbox_test.XXXXXX"};
            std::string fifo_path = concat_tostr(tmp_dir.path(), "fifo.pipe");
            throw_assert(mkfifo(fifo_path.c_str(), S_0644) == 0);
            sc.send_request(concat_tostr("echo > ", fifo_path));
            // Wait for the last command to execute (it may not finish)
            FileDescriptor fifo_reader{open(fifo_path.c_str(), O_RDONLY | O_CLOEXEC)};
            throw_assert(fifo_reader.is_open());
            char c;
            throw_assert(read_all(fifo_reader, &c, sizeof(c)) == 1);
        }
        sc.send_request("sleep infinity"); // we need an unfinished request, because await_result()
                                           // returns result of a completed request without error
        throw_assert(kill(supervisor_pid, SIGTERM) == 0);
        // Wait for supervisor process to die (if we don't the SupervisorConnection may see the
        // supervisor process as still alive because SIGTERM is not yet processed and will try to
        // kill the supervisor itself and won't report the unexpected death)
        throw_assert(syscalls::waitid(P_ALL, 0, nullptr, __WALL | WEXITED | WNOWAIT, nullptr) == 0);
        bool completed = false;
        try {
            throw_assert(std::get<sandbox::ResultOk>(sc.await_result()).system_result == 42 << 8);
            throw_assert(std::get<sandbox::ResultOk>(sc.await_result()).system_result == 43 << 8);
            completed = true;
            sc.await_result(
            ); // it may not fail if the command completed before stoping the supervisor
            sc.await_result(); // this will always fail if the previous did not
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
