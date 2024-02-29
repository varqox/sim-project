#include <cstdlib>
#include <gtest/gtest.h>
#include <regex>
#include <sched.h>
#include <simlib/concat_tostr.hh>
#include <simlib/defer.hh>
#include <simlib/file_contents.hh>
#include <sys/capability.h>

// NOLINTNEXTLINE
TEST(sandbox_external, capabiliites_and_user_namespaces) {
    auto uid = geteuid();
    auto gid = getegid();

    ASSERT_EQ(unshare(CLONE_NEWUSER), 0);
    put_file_contents("/proc/self/uid_map", from_unsafe{concat_tostr(uid, ' ', uid, " 1")});
    put_file_contents("/proc/self/setgroups", "deny");
    put_file_contents("/proc/self/gid_map", from_unsafe{concat_tostr(gid, ' ', gid, " 1")});

    // libcap is a horrible API to work with apart from the basic tasks, so we use simpler methods
    auto proc_self_status = get_file_contents("/proc/self/status");
    // Full sets
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapEff:\s+0*[137]f+\n)"}));
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapPrm:\s+0*[137]f+\n)"}));
    // Empty sets
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapInh:\s+0+\n)"}));
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapAmb:\s+0+\n)"}));

    // Drop capabilities
    cap_t caps = cap_init(); // no capabilities by default
    ASSERT_NE(caps, nullptr);
    ASSERT_EQ(cap_set_proc(caps), 0);
    ASSERT_EQ(cap_free(caps), 0);

    proc_self_status = get_file_contents("/proc/self/status");
    // Empty sets
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapEff:\s+0+\n)"}));
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapPrm:\s+0+\n)"}));
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapInh:\s+0+\n)"}));
    ASSERT_TRUE(std::regex_search(proc_self_status, std::regex{R"(\nCapAmb:\s+0+\n)"}));

    auto pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        if (unshare(CLONE_NEWUSER) != 0) {
            std::terminate();
        }
        put_file_contents("/proc/self/uid_map", from_unsafe{concat_tostr(uid, ' ', uid, " 1")});
        put_file_contents("/proc/self/setgroups", "deny");
        put_file_contents("/proc/self/gid_map", from_unsafe{concat_tostr(gid, ' ', gid, " 1")});

        proc_self_status = get_file_contents("/proc/self/status");
        // Full sets
        if (!std::regex_search(proc_self_status, std::regex{R"(\nCapEff:\s+0*[137]f+\n)"})) {
            std::terminate();
        }
        if (!std::regex_search(proc_self_status, std::regex{R"(\nCapPrm:\s+0*[137]f+\n)"})) {
            std::terminate();
        }
        // Empty sets
        if (!std::regex_search(proc_self_status, std::regex{R"(\nCapInh:\s+0+\n)"})) {
            std::terminate();
        }
        if (!std::regex_search(proc_self_status, std::regex{R"(\nCapAmb:\s+0+\n)"})) {
            std::terminate();
        }
        _exit(0);
    }
    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
