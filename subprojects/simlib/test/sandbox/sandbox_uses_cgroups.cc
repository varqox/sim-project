#include "../gtest_with_tester.hh"
#include "assert_result.hh"
#include "get_pid_of_the_only_child_process.hh"
#include "mount_operations_mount_proc_if_running_under_leak_sanitizer.hh"

#include <chrono>
#include <poll.h>
#include <simlib/file_descriptor.hh>
#include <simlib/file_info.hh>
#include <simlib/sandbox/sandbox.hh>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE
TEST(sandbox, test_each_of_pid1_and_tracee_gets_its_own_cgroup) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations =
                                    mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, removes_owned_cgroup) {
    std::string path;
    {
        auto sc = sandbox::spawn_supervisor({.cleanup_cgroup = true});
        ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/usr/bin/true"}})), CLD_EXITED, 0);
        auto cgroup = get_file_contents(
            concat_tostr("/proc/", get_pid_of_the_only_child_process(), "/cgroup")
        );
        path = "/sys/fs/cgroup" + cgroup.substr(cgroup.find("::") + 2);
        path.resize(path.rfind('/'));
        ASSERT_TRUE(path_exists(path));
    }
    ASSERT_FALSE(path_exists(path));
}

// NOLINTNEXTLINE
TEST(sandbox, removes_owned_cgroup_after_cancel) {
    std::string path;
    {
        auto sc = sandbox::spawn_supervisor({.cleanup_cgroup = true});
        ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/usr/bin/true"}})), CLD_EXITED, 0);
        auto cgroup = get_file_contents(
            concat_tostr("/proc/", get_pid_of_the_only_child_process(), "/cgroup")
        );
        path = "/sys/fs/cgroup" + cgroup.substr(cgroup.find("::") + 2);
        path.resize(path.rfind('/'));
        auto started_fd = FileDescriptor{eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)};
        ASSERT_TRUE(started_fd.is_open());
        auto request = sc.send_request({{"/usr/bin/sleep", "20"}}, {.started_fd = started_fd});
        auto pfd = pollfd{.fd = started_fd, .events = POLLIN, .revents = 0};
        ASSERT_EQ(poll(&pfd, 1, 10000), 1);
        ASSERT_TRUE(pfd.revents & POLLIN);
        request.cancel();
    }
    ASSERT_FALSE(path_exists(path));
}

// NOLINTNEXTLINE
TEST(sandbox, removes_owned_cgroup_after_setup_error) {
    std::string path;
    {
        auto sc = sandbox::spawn_supervisor({.cleanup_cgroup = true});
        auto result = sc.await_result(sc.send_request({{"/does-not-exist"}}));
        ASSERT_TRUE(std::holds_alternative<sandbox::result::Error>(result));
        auto cgroup = get_file_contents(
            concat_tostr("/proc/", get_pid_of_the_only_child_process(), "/cgroup")
        );
        path = "/sys/fs/cgroup" + cgroup.substr(cgroup.find("::") + 2);
        path.resize(path.rfind('/'));
    }
    ASSERT_FALSE(path_exists(path));
}

// NOLINTNEXTLINE
TEST(sandbox, nonblocking_request_does_not_wait_for_stopped_supervisor) {
    std::string path;
    {
        auto sc = sandbox::spawn_supervisor({.cleanup_cgroup = true, .nonblocking_requests = true});
        ASSERT_RESULT_OK(sc.await_result(sc.send_request({{"/usr/bin/true"}})), CLD_EXITED, 0);
        auto pid = get_pid_of_the_only_child_process();
        auto cgroup = get_file_contents(concat_tostr("/proc/", pid, "/cgroup"));
        path = "/sys/fs/cgroup" + cgroup.substr(cgroup.find("::") + 2);
        path.resize(path.rfind('/'));
        ASSERT_EQ(kill(pid, SIGSTOP), 0);
        siginfo_t si;
        ASSERT_EQ(waitid(P_PID, pid, &si, WSTOPPED), 0);
        auto arg = std::string(1 << 20, 'x');
        auto before = std::chrono::steady_clock::now();
        ASSERT_THROW((void)sc.send_request({{"/usr/bin/true", arg}}), std::exception);
        ASSERT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds{5});
    }
    ASSERT_FALSE(path_exists(path));
}
