#pragma once

#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

struct Socket {
    FileDescriptor our_end;
    FileDescriptor supervisor_end;
};

struct RunResult {
    std::string output;
    bool exited0;
};

inline RunResult run_supervisor(
    CStringView supervisor_executable_path, std::vector<std::string> args, Socket* sock_fd
) {
    // NOLINTNEXTLINE(android-cloexec-memfd-create)
    FileDescriptor output_fd{memfd_create("sandbox supervisor output", MFD_CLOEXEC)};
    throw_assert(output_fd.is_open());
    pid_t pid = fork();
    throw_assert(pid != -1);
    if (pid == 0) {
        if (sock_fd) {
            throw_assert(sock_fd->our_end.close() == 0);
        }
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args) {
            argv.emplace_back(arg.data());
        }
        argv.emplace_back(nullptr);
        throw_assert(dup3(output_fd, STDERR_FILENO, 0) == STDERR_FILENO);
        execve(supervisor_executable_path.c_str(), argv.data(), environ);
        _exit(1);
    }
    if (sock_fd) {
        throw_assert(sock_fd->supervisor_end.close() == 0);
        // Make supervisor exit immediately because of no awaiting requests
        throw_assert(sock_fd->our_end.close() == 0);
    }
    int status = 0;
    throw_assert(waitpid(pid, &status, 0) == pid);
    return {
        .output = get_file_contents(output_fd, 0, -1),
        .exited0 = WIFEXITED(status) && WEXITSTATUS(status) == 0,
    };
}
