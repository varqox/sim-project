#include "../gtest_with_tester.hh"
#include "assert_result.hh"
#include "mount_operations_mount_proc_if_running_under_leak_sanitizer.hh"

#include <chrono>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <optional>
#include <simlib/address_sanitizer.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/merge.hh>
#include <simlib/sandbox/sandbox.hh>
#include <unistd.h>

using sandbox::result::Ok;

// NOLINTNEXTLINE
TEST(sandbox, no_prlimit_limit) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations =
                                    mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// AddressSanitizer allocates ~14 TB of memory internally and restricting address space to 1 GiB
// causes it to fail
#if ADDRESS_SANITIZER
// NOLINTNEXTLINE
TEST(sandbox, DISABLED_prlimit_max_address_space_size_in_bytes) {
#else
// NOLINTNEXTLINE
TEST(sandbox, prlimit_max_address_space_size_in_bytes) {
#endif
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
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations =
                                    mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                            },
                    },
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
            .linux_namespaces =
                {
                    .mount =
                        {
                            .operations =
                                mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                        },
                },
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
            {{tester_executable_path, "file_size"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = merge(
                                    mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                                    Slice{{
                                        sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs{
                                            .path = "/dev",
                                            .max_total_size_of_files_in_bytes = std::nullopt,
                                            .inode_limit = 2,
                                            .read_only = false,
                                        },
                                    }}
                                ),
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
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations =
                                    mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                            },
                    },
                .prlimit =
                    {
                        .file_descriptors_num_limit = 3, // dynamic linker will fail
                    },
            }
        )),
        CLD_EXITED,
        127
    );

    // AddressSanitizer uses internally some file descriptors so that the test fails
    if constexpr (!ADDRESS_SANITIZER) {
        ASSERT_RESULT_OK(
            sc.await_result(sc.send_request(
                {{tester_executable_path, "file_descriptors_num"}},
                {
                    .stderr_fd = STDERR_FILENO,
                    .linux_namespaces =
                        {
                            .mount =
                                {
                                    .operations =
                                        mount_operations_mount_proc_if_running_under_leak_sanitizer(
                                        ),
                                },
                        },
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
}

// NOLINTNEXTLINE
TEST(sandbox, max_stack_size_in_bytes) {
    auto sc = sandbox::spawn_supervisor();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations =
                                    mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                            },
                    },
                .prlimit =
                    {
                        .max_stack_size_in_bytes = 0,
                    },
            }
        )),
        (OneOfSiCodes{CLD_DUMPED, CLD_KILLED}),
        SIGSEGV
    );
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "max_stack_size"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations =
                                    mount_operations_mount_proc_if_running_under_leak_sanitizer(),
                            },
                    },
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
