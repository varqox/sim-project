#include "find_cgroup_with_pid_as_only_process.hh"

#include <exception>
#include <simlib/file_contents.hh>
#include <simlib/file_manip.hh>
#include <simlib/noexcept_concat.hh>
#include <simlib/proc_status_file.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
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
        put_file_contents(tracee_cgroup_path + "/../cgroup.procs", "0");
        _exit(2);
    } catch (const std::exception& e) {
        if (!has_prefix(e.what(), "write() failed - No such file or directory (os error 2)")) {
            _exit(3);
        }
    }
    // Try to escape into a new sibling cgroup
    if (mkdir(tracee_cgroup_path + "/../abc")) {
        _exit(4);
    }
    try {
        put_file_contents(tracee_cgroup_path + "/../abc/cgroup.procs", "0");
        _exit(5);
    } catch (const std::exception& e) {
        if (!has_prefix(e.what(), "write() failed - No such file or directory (os error 2)")) {
            _exit(6);
        }
    }
}

void test_tracee_cgroup_path() {
    if (get_file_contents("/proc/self/cgroup") != "0::/\n") {
        _exit(7);
    }
}

void test_pid1_cgroup_path() {
    pid_t procfs_parent_pid =
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        str2num<pid_t>(from_unsafe{field_from_proc_status(
                           open("/proc/self/status", O_RDONLY | O_CLOEXEC), "PPid"
                       )})
            .value();
    ;
    if (get_file_contents(noexcept_concat("/proc/", procfs_parent_pid, "/cgroup")) !=
        "0::/../pid1\n")
    {
        _exit(8);
    }
}

int main() {
    test_escaping_from_tracee_cgroup();
    test_tracee_cgroup_path();
    test_pid1_cgroup_path();
}
