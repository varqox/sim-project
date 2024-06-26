#include <cerrno>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <simlib/file_perms.hh>
#include <simlib/macros/throw.hh>
#include <simlib/throw_assert.hh>
#include <simlib/to_arg_seq.hh>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
#include <vector>

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
    bool file_descriptors_num = false;
    bool max_stack_size = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (arg == "memory") {
            memory_limit = true;
        } else if (arg == "core_file_size") {
            core_file_size_limit = true;
        } else if (arg == "cpu_time") {
            cpu_time_limit = true;
        } else if (arg == "file_size") {
            file_size_limit = true;
        } else if (arg == "file_descriptors_num") {
            file_descriptors_num = true;
        } else if (arg == "max_stack_size") {
            max_stack_size = true;
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

        int fd = open("/dev/file", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_0644);
        throw_assert(fd >= 0);
        throw_assert(ftruncate(fd, 42) == 0);
        throw_assert(ftruncate(fd, 43) == -1 && errno == EFBIG);
        throw_assert(close(fd) == 0);

        // Create another file (the limit is not on the cumulative size)
        fd = open("/dev/another_file", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_0644);
        std::vector<uint8_t> data(42);
        throw_assert(getrandom(data.data(), data.size(), 0) == 42);
        throw_assert(write(fd, data.data(), data.size()) == 42);
        throw_assert(write(fd, "x", 1) == -1 && errno == EFBIG);
        throw_assert(close(fd) == 0);
    }
    if (file_descriptors_num) {
        auto rlim = get_prlimit(RLIMIT_NOFILE);
        throw_assert(rlim.rlim_cur == 4);
        throw_assert(rlim.rlim_max == 4);
        throw_assert(fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0) >= 0);
        throw_assert(fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0) == -1 && errno == EMFILE);
    }
    if (max_stack_size) {
        auto rlim = get_prlimit(RLIMIT_STACK);
        throw_assert(rlim.rlim_cur == 4123 << 10);
        throw_assert(rlim.rlim_max == 4123 << 10);
    }
}
