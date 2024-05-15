#include "can_create_children.hh"
#include "try_use_lots_of_memory.hh"

#include <simlib/leak_sanitizer.hh>
#include <simlib/macros/throw.hh>
#include <simlib/to_arg_seq.hh>

int main(int argc, char** argv) {
    bool pids_limit = false;
    bool check_memory_limit = false;
    bool cpu_max_bandwidth = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (arg == "pids_limit") {
            pids_limit = true;
        } else if (arg == "check_memory_limit") {
            check_memory_limit = true;
        } else if (arg == "cpu_max_bandwidth") {
            cpu_max_bandwidth = true;
        } else {
            THROW("unknown argument: ", arg);
        }
    }

    if (pids_limit != !can_create_children(1 + LEAK_SANITIZER)) {
        return 1;
    }
    if (cpu_max_bandwidth) {
        for (;;) {
        }
    }
    if (check_memory_limit) {
        try_use_lots_of_memory(3 << 20);
    }
}
