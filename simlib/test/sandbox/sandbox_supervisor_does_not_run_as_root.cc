#include "run_supervisor.hh"

#include <cstdlib>
#include <gtest/gtest.h>
#include <sched.h>
#include <simlib/concat_tostr.hh>
#include <simlib/file_contents.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <sys/mount.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static CStringView supervisor_executable_path;

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    throw_assert(argc == 2);
    supervisor_executable_path = CStringView{argv[1]};
    return RUN_ALL_TESTS();
}

// NOLINTNEXTLINE
TEST(sandbox_supervisor_does_not_run_as_root, does_not_run_with_uid_0) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto euid = geteuid();
        auto egid = getegid();
        unshare(CLONE_NEWUSER);
        put_file_contents("/proc/self/uid_map", from_unsafe{concat_tostr("0 ", euid, " 1")});
        put_file_contents("/proc/self/setgroups", "deny");
        put_file_contents("/proc/self/gid_map", from_unsafe{concat_tostr(egid, ' ', egid, " 1")});
        auto res = run_supervisor(supervisor_executable_path, {}, nullptr);
        throw_assert(res.output == "supervisor is not safe to be run by root");
        throw_assert(!res.exited0);
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(sandbox_supervisor_does_not_run_as_root, does_not_run_with_gid_0) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto euid = geteuid();
        auto egid = getegid();
        unshare(CLONE_NEWUSER);
        put_file_contents("/proc/self/uid_map", from_unsafe{concat_tostr(euid, ' ', euid, " 1")});
        put_file_contents("/proc/self/setgroups", "deny");
        put_file_contents("/proc/self/gid_map", from_unsafe{concat_tostr("0 ", egid, " 1")});
        auto res = run_supervisor(supervisor_executable_path, {}, nullptr);
        throw_assert(res.output == "supervisor is not safe to be run by root");
        throw_assert(!res.exited0);
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(sandbox_supervisor_does_not_run_as_root, runs_inside_user_namespace) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto euid = geteuid();
        auto egid = getegid();
        unshare(CLONE_NEWUSER);
        put_file_contents("/proc/self/uid_map", from_unsafe{concat_tostr(euid, ' ', euid, " 1")});
        put_file_contents("/proc/self/setgroups", "deny");
        put_file_contents("/proc/self/gid_map", from_unsafe{concat_tostr(egid, ' ', egid, " 1")});
        auto res = run_supervisor(supervisor_executable_path, {}, nullptr);
        throw_assert(
            res.output == "Usage: sandbox-supervisor <unix socket file descriptor number>"
        );
        throw_assert(!res.exited0);
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(sandbox_supervisor_does_not_run_as_root, does_not_run_without_uid_map) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto egid = getegid();
        unshare(CLONE_NEWUSER);
        put_file_contents("/proc/self/setgroups", "deny");
        put_file_contents("/proc/self/gid_map", from_unsafe{concat_tostr(egid, ' ', egid, " 1")});
        auto res = run_supervisor(supervisor_executable_path, {}, nullptr);
        throw_assert(res.output == "supervisor is not safe to be run by root");
        throw_assert(!res.exited0);
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(sandbox_supervisor_does_not_run_as_root, does_not_run_without_gid_map) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto euid = geteuid();
        unshare(CLONE_NEWUSER);
        put_file_contents("/proc/self/uid_map", from_unsafe{concat_tostr(euid, ' ', euid, " 1")});
        auto res = run_supervisor(supervisor_executable_path, {}, nullptr);
        throw_assert(res.output == "supervisor is not safe to be run by root");
        throw_assert(!res.exited0);
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// NOLINTNEXTLINE
TEST(sandbox_supervisor_does_not_run_as_root, does_not_run_with_tampered_dev_null) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        auto euid = geteuid();
        auto egid = getegid();
        unshare(CLONE_NEWUSER | CLONE_NEWNS);
        put_file_contents("/proc/self/uid_map", from_unsafe{concat_tostr(euid, ' ', euid, " 1")});
        put_file_contents("/proc/self/setgroups", "deny");
        put_file_contents("/proc/self/gid_map", from_unsafe{concat_tostr(egid, ' ', egid, " 1")});
        throw_assert(mount("/dev/zero", "/dev/null", nullptr, MS_BIND, nullptr) == 0);
        auto res = run_supervisor(supervisor_executable_path, {}, nullptr);
        throw_assert(res.output == "supervisor is not safe to be run by root");
        throw_assert(!res.exited0);
        _exit(0);
    }
    // Reap child
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
