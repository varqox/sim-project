#include "find_cgroup_with_pid_as_only_process.hh"

#include <exception>
#include <regex>
#include <simlib/file_manip.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/read_file.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/throw_assert.hh>
#include <simlib/write_file.hh>
#include <unistd.h>

void test_escaping_from_tracee_cgroup() {
    auto path = std::string{"/sys/fs/cgroup"};
    auto tracee_cgroup_path_opt = find_cgroup_with_pid_as_only_process(path, getpid());
    if (!tracee_cgroup_path_opt) {
        _exit(1);
    }
    auto tracee_cgroup_path = *tracee_cgroup_path_opt;
    // Try to escape into the parent cgroup
    try {
        write_file(tracee_cgroup_path + "/../cgroup.procs", "0");
        _exit(2);
    } catch (const std::exception& e) {
        if (!has_prefix(e.what(), "write() - No such file or directory (os error 2)")) {
            _exit(3);
        }
    }
    // Try to escape into a new sibling cgroup
    if (mkdir(tracee_cgroup_path + "/../abc")) {
        _exit(4);
    }
    try {
        write_file(tracee_cgroup_path + "/../abc/cgroup.procs", "0");
        _exit(5);
    } catch (const std::exception& e) {
        if (!has_prefix(e.what(), "write() - No such file or directory (os error 2)")) {
            _exit(6);
        }
    }
}

void test_tracee_cgroup_path() {
    if (read_file("/proc/self/cgroup") != "0::/\n") {
        _exit(7);
    }
}

void test_pid1_cgroup_path() {
    pid_t procfs_parent_pid = [] {
        auto proc_self_status_contents = read_file("/proc/self/status");
        std::smatch parts;
        throw_assert(
            std::regex_search(proc_self_status_contents, parts, std::regex{"\nPPid:\\s*(\\d*)"})
        );
        return str2num<pid_t>(from_unsafe{parts[1].str()}).value();
    }();
    if (read_file(noexcept_concat("/proc/", procfs_parent_pid, "/cgroup")) != "0::/../pid1\n") {
        _exit(8);
    }
}

int main() {
    test_escaping_from_tracee_cgroup();
    test_tracee_cgroup_path();
    test_pid1_cgroup_path();
}
