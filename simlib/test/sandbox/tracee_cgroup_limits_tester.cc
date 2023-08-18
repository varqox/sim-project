#include <cerrno>
#include <simlib/errmsg.hh>
#include <simlib/macros/throw.hh>
#include <simlib/to_arg_seq.hh>
#include <sys/mman.h>
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

// This will SIGKILL the process rather than failing allocation
void try_use_lots_of_memory() {
    if (mmap(
            nullptr,
            3 << 20,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE,
            -1,
            0
        ) == MAP_FAILED) // NOLINT(performance-no-int-to-ptr)
    {
        THROW("mmap()", errmsg());
    }
}

int main(int argc, char** argv) {
    bool pids_limit = false;
    bool check_memory_limit = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (arg == "pids_limit") {
            pids_limit = true;
        } else if (arg == "check_memory_limit") {
            check_memory_limit = true;
        } else {
            THROW("unknown argument: ", arg);
        }
    }

    if (pids_limit != !can_create_child()) {
        return 1;
    }

    if (check_memory_limit) {
        try_use_lots_of_memory();
    }
}
