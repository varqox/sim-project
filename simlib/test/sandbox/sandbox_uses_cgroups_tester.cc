#include "find_cgroup_with_pid_as_only_process.hh"

#include <string>
#include <unistd.h>

void test_tracee_gets_its_cgroup() {
    auto path = std::string{"/sys/fs/cgroup"};
    static constexpr pid_t tracee_pid = 2;
    if (getpid() != tracee_pid) {
        _exit(1);
    }
    if (!find_cgroup_with_pid_as_only_process(path, tracee_pid)) {
        _exit(2);
    }
}

void test_pid1_gets_its_cgroup() {
    auto path = std::string{"/sys/fs/cgroup"};
    static constexpr pid_t parent_pid = 1;
    if (getppid() != parent_pid) {
        _exit(3);
    }
    if (!find_cgroup_with_pid_as_only_process(path, parent_pid)) {
        _exit(4);
    }
}

int main() {
    test_tracee_gets_its_cgroup();
    test_pid1_gets_its_cgroup();
}
