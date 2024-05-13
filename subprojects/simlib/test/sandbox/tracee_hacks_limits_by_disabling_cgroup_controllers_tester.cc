#include "can_create_children.hh"
#include "find_cgroup_with_pid_as_only_process.hh"
#include "try_use_lots_of_memory.hh"

#include <simlib/file_contents.hh>
#include <simlib/leak_sanitizer.hh>
#include <simlib/throw_assert.hh>
#include <string>
#include <unistd.h>

int main() {
    throw_assert(!can_create_children(1 + LEAK_SANITIZER)); // limits work
    auto path = std::string{"/sys/fs/cgroup"};
    auto tracee_cgroup_path_opt = find_cgroup_with_pid_as_only_process(path, getpid());
    if (!tracee_cgroup_path_opt) {
        return 1;
    }
    auto tracee_cgroup_path = *tracee_cgroup_path_opt;
    put_file_contents(tracee_cgroup_path + "/../cgroup.subtree_control", "-pids -memory");

    // Check that controllers disabled successfully
    throw_assert(can_create_children(1 + LEAK_SANITIZER));
    try_use_lots_of_memory(33 << 20);

    // Enable controllers for the supervisor to not error-out
    put_file_contents(tracee_cgroup_path + "/../cgroup.subtree_control", "+pids +memory");
    return 0;
}
