#pragma once

#include <simlib/leak_sanitizer.hh>
#include <simlib/sandbox/sandbox.hh>
#include <vector>

inline std::vector<sandbox::RequestOptions::LinuxNamespaces::Mount::Operation>
mount_operations_mount_proc_if_running_under_leak_sanitizer() {
    if (!LEAK_SANITIZER) {
        return {};
    }
    return {
        sandbox::RequestOptions::LinuxNamespaces::Mount::MountProc{
            .path = "/proc",
            .read_only = true,
        },
    };
}
