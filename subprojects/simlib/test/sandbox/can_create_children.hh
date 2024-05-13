#pragma once

#include <cerrno>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

inline bool can_create_children(unsigned n) {
    std::vector<pid_t> pids(n, -1);
    auto await_children = [&pids] {
        int errnum = 0;
        for (auto pid : pids) {
            if (pid == -1) {
                break;
            }
            if (waitpid(pid, nullptr, 0) != pid) {
                errnum = errno;
            }
        }
        if (errnum) {
            THROW("waitpid()", errmsg(errnum));
        }
    };


    for (auto& pid : pids) {
        pid = fork();
        if (pid == -1) {
            int errnum = errno;
            await_children();
            if (errnum != EAGAIN) {
                THROW("fork()", errmsg(errnum));
            }
            return false;
        }
        if (pid == 0) {
            _exit(0);
        }
    }
    await_children();
    return true;
}
