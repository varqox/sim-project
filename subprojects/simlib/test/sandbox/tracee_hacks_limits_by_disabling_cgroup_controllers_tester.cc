#include "can_create_children.hh"
#include "find_cgroup_with_pid_as_only_process.hh"
#include "try_use_lots_of_memory.hh"

#include <simlib/leak_sanitizer.hh>
#include <simlib/throw_assert.hh>
#include <simlib/write_file.hh>
#include <string>
#include <unistd.h>

int main() {
    throw_assert(!can_create_children(1)); // limits work
    auto path = std::string{"/sys/fs/cgroup"};
    auto tracee_cgroup_path_opt = find_cgroup_with_pid_as_only_process(path, getpid());
    if (!tracee_cgroup_path_opt) {
        return 1;
    }
    auto tracee_cgroup_path = *tracee_cgroup_path_opt;
    write_file(tracee_cgroup_path + "/../cgroup.subtree_control", "-pids -memory");

    // Check that controllers disabled successfully
    throw_assert(can_create_children(1));
    try_use_lots_of_memory(65 << 20);

    // Enable controllers for the supervisor to not error-out
    write_file(tracee_cgroup_path + "/../cgroup.subtree_control", "+pids +memory");
    return 0;
}
