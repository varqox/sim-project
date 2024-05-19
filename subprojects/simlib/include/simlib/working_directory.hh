#pragma once

#include <climits>
#include <exception>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/macros/throw.hh>
#include <unistd.h>

class DirectoryChanger {
    FileDescriptor old_cwd;

public:
    // Does NOT change current working directory
    DirectoryChanger() = default;

    explicit DirectoryChanger(FilePath new_wd) : old_cwd(".", O_RDONLY | O_CLOEXEC) {
        if (old_cwd == -1) {
            THROW("open() failed", errmsg());
        }

        if (chdir(new_wd.data()) == -1) {
            auto err = errno;
            THROW("chdir() failed", errmsg(err));
        }
    }

    DirectoryChanger(const DirectoryChanger&) = delete;
    DirectoryChanger(DirectoryChanger&&) noexcept = default;
    DirectoryChanger& operator=(const DirectoryChanger&) = delete;
    DirectoryChanger& operator=(DirectoryChanger&&) noexcept = default;

    ~DirectoryChanger() {
        if (old_cwd >= 0) {
            if (fchdir(old_cwd)) {
                std::terminate();
            }
        }
    }
};

/**
 * @brief Get current working directory
 * @details Uses get_current_dir_name()
 *
 * @return current working directory (absolute path, with trailing '/')
 *
 * @errors If get_current_dir_name() fails then std::runtime_error will be
 *   thrown
 */
InplaceBuff<PATH_MAX> get_cwd();

/**
 * @brief Change current working directory to the @p path interpreted relative
 *   to the process's executable path directory
 * @details Uses executable_path() and chdir(2)
 *
 * @param path path interpreted relative to the current process's executable
 *   path's directory's path, @p path == "" is equivalent to @p path == "."
 *
 * @errors Exceptions from executable_path() or if chdir(2) fails then
 *   std::runtime_error will be thrown
 */
void chdir_relative_to_executable_dirpath(StringView path = ".");
