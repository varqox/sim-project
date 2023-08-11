#pragma once

#include <cerrno>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <sys/wait.h>
#include <unistd.h>

inline bool can_create_child() {
    auto pid = fork();
    if (pid == -1) {
        if (errno != EAGAIN) {
            THROW("fork()", errmsg());
        }
        return false;
    }
    if (pid == 0) {
        _exit(0);
    }
    if (waitpid(pid, nullptr, 0) != pid) {
        THROW("waitpid()", errmsg());
    }
    return true;
}
