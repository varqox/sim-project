#include <cerrno>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <simlib/to_arg_seq.hh>
#include <sys/wait.h>
#include <unistd.h>

bool can_create_child() {
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

int main(int argc, char** argv) {
    bool pids_limit = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (arg == "pids_limit") {
            pids_limit = true;
        } else {
            THROW("unknown argument: ", arg);
        }
    }

    if (pids_limit != !can_create_child()) {
        return 1;
    }
}
