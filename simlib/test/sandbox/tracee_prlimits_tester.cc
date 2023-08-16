#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <simlib/file_contents.hh>
#include <simlib/macros/throw.hh>
#include <simlib/random_bytes.hh>
#include <simlib/throw_assert.hh>
#include <simlib/to_arg_seq.hh>
#include <sys/mman.h>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>

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
    bool file_size_limit = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (arg == "memory") {
            memory_limit = true;
        } else if (arg == "core_file_size") {
            core_file_size_limit = true;
        } else if (arg == "cpu_time") {
            cpu_time_limit = true;
        } else if (arg == "file_size") {
            file_size_limit = true;
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
    if (file_size_limit) {
        auto rlim = get_prlimit(RLIMIT_FSIZE);
        throw_assert(rlim.rlim_cur == 42);
        throw_assert(rlim.rlim_max == 42);

        throw_assert(signal(SIGXFSZ, SIG_IGN) != SIG_ERR); // NOLINT(performance-no-int-to-ptr)

        int fd = open("/tmp/file", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC);
        throw_assert(fd >= 0);
        throw_assert(ftruncate(fd, 42) == 0);
        throw_assert(ftruncate(fd, 43) == -1 && errno == EFBIG);
        throw_assert(close(fd) == 0);

        // Create another file (the limit is not on the cumulative size)
        fd = open("/tmp/another_file", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC);
        throw_assert(write_all(fd, from_unsafe{random_bytes(42)}) == 42);
        throw_assert(write_all(fd, "x", 1) == 0 && errno == EFBIG);
        throw_assert(close(fd) == 0);
    }
}
