#include "fresh_execution.hh"
#include "test_compiled_language_suite.hh"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <pthread.h>
#include <simlib/defer.hh>
#include <simlib/file_info.hh>
#include <simlib/pipe.hh>
#include <simlib/sim/judge/language_suite/c_gcc.hh>
#include <sys/socket.h>
#include <sys/un.h>

using sim::judge::language_suite::C_GCC;

constexpr auto test_prog_ok = R"(
#include <stdio.h>

int main() {
    printf("%i\n", 42);
}
)";

constexpr auto test_prog_invalid = R"(
int main() {
    p;
}
)";

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_c11) {
    auto suite = C_GCC{C_GCC::Standard::C11};
    ASSERT_EQ(suite.is_supported(), path_exists("/usr/bin/gcc"));
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "error: 'p' undeclared"
        );
    }
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_c23) {
    auto suite = C_GCC{C_GCC::Standard::C23};
    ASSERT_EQ(suite.is_supported(), path_exists("/usr/bin/gcc"));
    if (suite.is_supported()) {
        test_compiled_language_suite(
            suite, test_prog_ok, test_prog_invalid, "error: 'p' undeclared"
        );
    }
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_fresh_execution_isolates_ipc) {
    auto suite = C_GCC{C_GCC::Standard::C11};
    compile_for_fresh_execution(suite, R"(
#include <errno.h>
#include <sys/sem.h>
int main(int argc, char** argv) {
    if (argv[1][0] == 's') {
        int sem = semget(173241, 1, 0600 | IPC_CREAT | IPC_EXCL);
        return sem < 0 || semctl(sem, 0, SETVAL, 42) < 0;
    }
    return semget(173241, 1, 0600) != -1 || errno != ENOENT;
}
)");
    auto program = suite.prepared_program();
    auto first = program.start_fresh({{"store"}}, fresh_execution_options());
    ASSERT_EQ(first.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
    auto second = program.start_fresh({{"load"}}, fresh_execution_options());
    ASSERT_EQ(second.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_prepared_program_owns_executable) {
    auto program = [] {
        auto suite = C_GCC{C_GCC::Standard::C11};
        compile_for_fresh_execution(suite, "int main() { return 42; }\n");
        auto prepared = suite.prepared_program();
        compile_for_fresh_execution(suite, "int main() { return 17; }\n");
        return prepared;
    }();
    auto execution = program.start_fresh({}, fresh_execution_options());
    ASSERT_EQ(execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 42}));
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_cannot_prepare_without_compilation) {
    auto suite = C_GCC{C_GCC::Standard::C11};
    ASSERT_THROW((void)suite.prepared_program(), std::exception);
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_fresh_execution_resets_signal_mask) {
    auto suite = C_GCC{C_GCC::Standard::C11};
    compile_for_fresh_execution(suite, R"(
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
int main() {
    sigset_t mask;
    if (sigprocmask(SIG_BLOCK, 0, &mask)) return 1;
    if (sigismember(&mask, SIGPIPE)) return 2;
    if (sigismember(&mask, SIGUSR2)) return 3;
    return 0;
}
)");
    auto program = suite.prepared_program();
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigaddset(&mask, SIGUSR2);
    sigset_t old_mask;
    ASSERT_EQ(pthread_sigmask(SIG_BLOCK, &mask, &old_mask), 0);
    Defer restore_mask = [&] { (void)pthread_sigmask(SIG_SETMASK, &old_mask, nullptr); };
    auto execution = program.start_fresh({}, fresh_execution_options());
    ASSERT_EQ(execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_fresh_execution_has_no_inherited_descriptors) {
    auto secret = TemporaryFile{"/tmp/sim_fresh_execution_secret.XXXXXX"};
    put_file_contents(secret.path(), "host secret\n");
    auto inherited = FileDescriptor{secret.path(), O_RDONLY};
    ASSERT_TRUE(inherited.is_open());
    ASSERT_EQ(fcntl(inherited, F_SETFD, 0), 0);
    auto suite = C_GCC{C_GCC::Standard::C11};
    compile_for_fresh_execution(suite, R"(
#include <errno.h>
#include <fcntl.h>
int main() {
    for (int fd = 3; fd < 4096; ++fd) {
        errno = 0;
        if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) return 1;
    }
    return 0;
}
)");
    auto program = suite.prepared_program();
    auto first = program.start_fresh({}, fresh_execution_options());
    auto second = program.start_fresh({}, fresh_execution_options());
    ASSERT_EQ(first.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
    ASSERT_EQ(second.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_fresh_execution_restricts_filesystem_and_environment) {
    auto secret = TemporaryFile{"/tmp/sim_fresh_execution_secret.XXXXXX"};
    put_file_contents(secret.path(), "host secret\n");
    auto suite = C_GCC{C_GCC::Standard::C11};
    compile_for_fresh_execution(suite, R"(
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/ptrace.h>
#include <unistd.h>
extern char **environ;
int main(int argc, char **argv) {
    if (open(argv[1], O_RDONLY) != -1) return 1;
    if (open("/in", O_RDONLY) != -1) return 2;
    if (open("/proc/self/fd", O_RDONLY) != -1) return 3;
    if (open("/exe", O_WRONLY) != -1) return 4;
    if (open("/created", O_CREAT | O_WRONLY, 0600) != -1) return 5;
    if (*environ) return 6;
    if (getuid() == 0 || geteuid() == 0) return 7;
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) != -1 || errno != ENOSYS) return 8;
    if (mount(0, "/", "tmpfs", 0, 0) != -1 || errno != ENOSYS) return 9;
    return 0;
}
)");
    auto execution =
        suite.prepared_program().start_fresh({{secret.path()}}, fresh_execution_options());
    ASSERT_EQ(execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_fresh_execution_restricts_process_creation) {
    auto suite = C_GCC{C_GCC::Standard::C11};
    compile_for_fresh_execution(suite, R"(
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
int main() {
    pid_t child = fork();
    if (child == 0) _exit(0);
    if (child > 0) {
        waitpid(child, 0, 0);
        return 1;
    }
    return errno != EAGAIN;
}
)");
    auto execution = suite.prepared_program().start_fresh({}, fresh_execution_options());
    ASSERT_EQ(execution.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}

// NOLINTNEXTLINE
TEST(sim_judge_compiler, c_gcc_fresh_execution_isolates_network) {
    auto name = concat_tostr("sim_fresh_network_", getpid());
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::copy(name.begin(), name.end(), address.sun_path + 1);
    auto address_size = offsetof(sockaddr_un, sun_path) + 1 + name.size();
    auto host_socket = FileDescriptor{socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
    ASSERT_TRUE(host_socket.is_open());
    ASSERT_EQ(bind(host_socket, reinterpret_cast<sockaddr*>(&address), address_size), 0);
    ASSERT_EQ(listen(host_socket, 1), 0);
    auto suite = C_GCC{C_GCC::Standard::C11};
    compile_for_fresh_execution(suite, R"(
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
int main(int argc, char **argv) {
    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path + 1, argv[1], strlen(argv[1]));
    socklen_t size = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(argv[1]);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 1;
    if (connect(fd, (struct sockaddr *)&address, size) != -1) return 2;
    if (errno != ECONNREFUSED) return 3;
    close(fd);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 4;
    if (bind(fd, (struct sockaddr *)&address, size)) return 5;
    if (listen(fd, 1)) return 6;
    if (write(1, "r", 1) != 1) return 7;
    char byte;
    return read(0, &byte, 1) != 1 || byte != 'x';
}
)");
    auto program = suite.prepared_program();
    auto input1 = pipe2(O_CLOEXEC).value();
    auto input2 = pipe2(O_CLOEXEC).value();
    auto output1 = pipe2(O_CLOEXEC).value();
    auto output2 = pipe2(O_CLOEXEC).value();
    auto options = fresh_execution_options();
    options.stdin_fd = input1.readable;
    options.stdout_fd = output1.writable;
    auto first = program.start_fresh({{name}}, options);
    ASSERT_TRUE(readable_within(output1.readable, std::chrono::seconds{10}));
    ASSERT_EQ(get_file_contents(output1.readable, 1), "r");
    options.stdin_fd = input2.readable;
    options.stdout_fd = output2.writable;
    auto second = program.start_fresh({{name}}, options);
    ASSERT_TRUE(readable_within(output2.readable, std::chrono::seconds{10}));
    ASSERT_EQ(get_file_contents(output2.readable, 1), "r");
    ASSERT_EQ(write(input1.writable, "x", 1), 1);
    ASSERT_EQ(write(input2.writable, "x", 1), 1);
    ASSERT_EQ(first.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
    ASSERT_EQ(second.await_result().si, (sandbox::Si{.code = CLD_EXITED, .status = 0}));
}
