#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <chrono>
#include <fcntl.h>
#include <optional>
#include <simlib/file_descriptor.hh>
#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, no_prlimit_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request({{tester_executable_path}}, {.stderr_fd = STDERR_FILENO})),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, prlimit_max_address_space_size_in_bytes) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "memory"}},
            {
                .stderr_fd = STDERR_FILENO,
                .prlimit =
                    {
                        .max_address_space_size_in_bytes = 1 << 30,
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, max_core_file_size_in_bytes) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "core_file_size"}},
            {
                .stderr_fd = STDERR_FILENO,
                .prlimit =
                    {
                        .max_core_file_size_in_bytes = 0,
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, cpu_time_limit_in_seconds) {
    auto sc = sandbox::spawn_supervisor();
    auto res = sc.await_result(sc.send_request(
        {{tester_executable_path, "cpu_time"}},
        {
            .stderr_fd = STDERR_FILENO,
            .prlimit =
                {
                    .cpu_time_limit_in_seconds = 1,
                },
        }
    ));
    ASSERT_RESULT_OK(res, CLD_KILLED, SIGKILL);
    ASSERT_GE(
        std::get<Ok>(res).cgroup.cpu_time.total(), std::chrono::milliseconds{800}
    ); // Cgroups count CPU time slightly differently
}

// NOLINTNEXTLINE
TEST(sandbox, max_file_size_in_bytes) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            FileDescriptor{tester_executable_path.data(), O_RDONLY},
            {{tester_executable_path, "file_size"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs{
                                        .path = "/tmp",
                                        .max_total_size_of_files_in_bytes = std::nullopt,
                                        .inode_limit = 2,
                                        .read_only = false,
                                    },
                                }},
                            },
                    },
                .prlimit =
                    {
                        .max_file_size_in_bytes = 42,
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, file_descriptors_num_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
            {
                .stderr_fd = STDERR_FILENO,
                .prlimit =
                    {
                        .file_descriptors_num_limit = 3, // dynamic linker will fail
                    },
            }
        )),
        CLD_EXITED,
        127
    );
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "file_descriptors_num"}},
            {
                .stderr_fd = STDERR_FILENO,
                .prlimit =
                    {
                        .file_descriptors_num_limit = 4,
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, max_stack_size_in_bytes) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
            {
                .stderr_fd = STDERR_FILENO,
                .prlimit =
                    {
                        .max_stack_size_in_bytes = 0,
                    },
            }
        )),
        CLD_KILLED,
        SIGSEGV
    );
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "max_stack_size"}},
            {
                .stderr_fd = STDERR_FILENO,
                .prlimit =
                    {
                        .max_stack_size_in_bytes = 4123 << 10,
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}
