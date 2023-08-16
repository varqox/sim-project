#include <cerrno>
#include <simlib/macros/throw.hh>
#include <simlib/throw_assert.hh>
#include <simlib/to_arg_seq.hh>
#include <sys/mman.h>
#include <sys/resource.h>
#include <thread>

template <class T>
rlimit64 get_prlimit(T resource) {
    rlimit64 rlim;
    if (prlimit64(0, resource, nullptr, &rlim)) {
        THROW("prlimit64()");
    }
    return rlim;
}

[[nodiscard]] bool mmap_fails(size_t size_in_bytes) {
    if (mmap(nullptr, size_in_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) ==
        MAP_FAILED) // NOLINT(performance-no-int-to-ptr)
    {
        throw_assert(errno == ENOMEM);
        return true;
    }
    return false;
}

int main(int argc, char** argv) {
    bool memory_limit = false;
    bool core_file_size_limit = false;
    bool cpu_time_limit = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (arg == "memory") {
            memory_limit = true;
        } else if (arg == "core_file_size") {
            core_file_size_limit = true;
        } else if (arg == "cpu_time") {
            cpu_time_limit = true;
        } else {
            THROW("Unrecognized argument: ", arg);
        }
    }

    throw_assert(memory_limit == mmap_fails(1 << 30));
    if (memory_limit) {
        auto rlim = get_prlimit(RLIMIT_AS);
        throw_assert(rlim.rlim_cur == 1 << 30);
        throw_assert(rlim.rlim_max == 1 << 30);
    }
    if (core_file_size_limit) {
        auto rlim = get_prlimit(RLIMIT_CORE);
        throw_assert(rlim.rlim_cur == 0);
        throw_assert(rlim.rlim_max == 0);
    }
    if (cpu_time_limit) {
        auto rlim = get_prlimit(RLIMIT_CPU);
        throw_assert(rlim.rlim_cur == 1);
        throw_assert(rlim.rlim_max == 1);
        // Spawn threads to faster consume the cpu time limit
        for (size_t i = 1; i < std::thread::hardware_concurrency(); ++i) {
            std::thread{[] {
                for (;;) {
                }
            }}.detach();
        }
        for (;;) {
        }
    }
}
