#include <cctype>
#include <csignal>
#include <exception>
#include <gtest/gtest.h>
#include <simlib/concat_tostr.hh>
#include <simlib/file_contents.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/syscalls.hh>
#include <simlib/throw_assert.hh>
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
