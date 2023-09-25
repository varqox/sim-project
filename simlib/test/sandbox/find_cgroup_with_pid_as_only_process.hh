#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <simlib/directory.hh>
#include <simlib/errmsg.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/repeating.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <string>
#include <unistd.h>

inline std::optional<std::string>
find_cgroup_with_pid_as_only_process(std::string& path, pid_t pid) {
    auto cgroup_procs_fd =
        FileDescriptor{open((path + "/cgroup.procs").c_str(), O_RDONLY | O_CLOEXEC)};
    if (cgroup_procs_fd < 0) {
        if (errno == ENOENT || errno == ENODEV) {
            return std::nullopt; // cgroup just disappeared
        }
        THROW("open()", errmsg());
    }

    {
        std::array<char, 32> buff;
        auto len = read_all(cgroup_procs_fd, buff.data(), buff.size());
        if (errno == ENODEV) {
            return std::nullopt; // cgorup just disappeared
        }
        if (errno) {
            THROW("read()", errmsg());
        }
        if (str2num<pid_t>(StringView{buff.data(), len}.without_trailing('\n')) == pid) {
            return path;
        }
    }

    std::optional<std::string> found;
    auto dir = Directory{path};
    if (!dir.is_open()) {
        return std::nullopt; // cgorup just disappeared
    }
    path += '/';
    for_each_dir_component(dir, [&](dirent* entry) {
        if (entry->d_type == DT_DIR) {
            size_t saved_path_size = path.size();
            path += entry->d_name;
            found = find_cgroup_with_pid_as_only_process(path, pid);
            path.resize(saved_path_size);
            if (found) {
                return repeating::STOP;
            }
        }
        return repeating::CONTINUE;
    });
    path.pop_back();
    return found;
}
