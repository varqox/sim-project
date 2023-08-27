#pragma once

#include <memory>
#include <optional>
#include <sys/types.h>
#include <vector>

namespace sandbox::supervisor::request {

struct Request {
    int executable_fd;
    std::optional<int> stdin_fd;
    std::optional<int> stdout_fd;
    std::optional<int> stderr_fd;
    std::unique_ptr<std::byte[]> buff;
    std::vector<char*> argv; // with a trailing nullptr element
    std::vector<char*> env; // with a trailing nullptr element

    struct LinuxNamespaces {
        struct User {
            std::optional<uid_t> inside_uid;
            std::optional<gid_t> inside_gid;
        } user;
    } linux_namespaces;

    struct Cgroup {
        std::optional<uint32_t> process_num_limit;
        std::optional<uint64_t> memory_limit_in_bytes;
    } cgroup;
};

} // namespace sandbox::supervisor::request
