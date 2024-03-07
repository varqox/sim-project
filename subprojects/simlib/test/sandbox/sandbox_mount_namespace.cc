#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <exception>
#include <fcntl.h>
#include <gmock/gmock.h>
#include <optional>
#include <simlib/concat_tostr.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/string_view.hh>
#include <stdexcept>

using std::nullopt;
using MountTmpfs = sandbox::RequestOptions::LinuxNamespaces::Mount::MountTmpfs;
using MountProc = sandbox::RequestOptions::LinuxNamespaces::Mount::MountProc;
using BindMount = sandbox::RequestOptions::LinuxNamespaces::Mount::BindMount;
using CreateFile = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateFile;
using CreateDir = sandbox::RequestOptions::LinuxNamespaces::Mount::CreateDir;

sandbox::SupervisorConnection& get_sc() {
    static auto sc = sandbox::spawn_supervisor();
    return sc;
}

// NOLINTNEXTLINE
TEST(sandbox, no_operations_no_new_root_mount_path) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{tester_executable_path, "no_operations_no_new_root_mount_path"}},
            {.stderr_fd = STDERR_FILENO}
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, no_operations_new_root_mount_path) {
    auto& sc = get_sc();
    // pivot_root()'s new_root has to be a mount point in the mount namespace it is called in. Since
    // we specified no operations, there are no mount points in the new mount namespace, therefore
    // pivot_root() fails
    ASSERT_RESULT_ERROR(
        sc.await_result(sc.send_request(
            {{"/bin/true"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .new_root_mount_path = "/",
                            },
                    },
            }
        )),
        "pid1: pivot_root(\".\", \".\") - Invalid argument (os error 22)"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, new_root_mount_path) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{"/usr/bin/bash", "-c", "test ! -e /tmp"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/sys/hypervisor",
                                        .inode_limit = nullopt,
                                        .read_only = false
                                    },
                                    CreateDir{.path = "/sys/hypervisor/usr"},
                                    BindMount{
                                        .source = "/usr",
                                        .dest = "/sys/hypervisor/usr",
                                        .no_exec = false,
                                    },
                                    CreateDir{.path = "/sys/hypervisor/lib"},
                                    BindMount{
                                        .source = "/lib",
                                        .dest = "/sys/hypervisor/lib",
                                        .no_exec = false,
                                    },
                                    CreateDir{.path = "/sys/hypervisor/lib64"},
                                    BindMount{
                                        .source = "/lib64",
                                        .dest = "/sys/hypervisor/lib64",
                                        .no_exec = false,
                                    },
                                }},
                                .new_root_mount_path = "/sys/hypervisor",
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, new_root_mount_path_equals_root) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            {{"/usr/bin/bash", "-c", "test ! -e /tmp"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/", .inode_limit = nullopt, .read_only = false
                                    },
                                    CreateDir{.path = "/../usr"},
                                    BindMount{
                                        .source = "/usr",
                                        .dest = "/../usr",
                                        .no_exec = false,
                                    },
                                    CreateDir{.path = "/../lib"},
                                    BindMount{
                                        .source = "/lib",
                                        .dest = "/../lib",
                                        .no_exec = false,
                                    },
                                    CreateDir{.path = "/../lib64"},
                                    BindMount{
                                        .source = "/lib64",
                                        .dest = "/../lib64",
                                        .no_exec = false,
                                    },
                                }},
                                .new_root_mount_path = "/..",
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, mount_tmpfs) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc
            .await_result(sc
                              .send_request(
                                  FileDescriptor{tester_executable_path.data(), O_RDONLY},
                                  {{tester_executable_path, "mount_tmpfs"}},
                                  {
                                      .stderr_fd = STDERR_FILENO,
                                      .linux_namespaces =
                                          {
                                              .mount =
                                                  {
                                                      .operations =
                                                          {
                                                              {
                                                                  MountTmpfs{
                                                                      .path = "/tmp",
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_nullopt"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_nullopt",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          std::nullopt,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_0"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_0",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          0,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_1"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_1",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          1,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_32768"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "max_total_size_of_"
                                                                              "files_32768",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          32768,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "inode_limit_"
                                                                              "nullopt"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "inode_limit_nullopt",
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/inode_limit_0"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/inode_limit_0",
                                                                      .inode_limit = 0,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/inode_limit_1"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/inode_limit_1",
                                                                      .inode_limit = 1,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/inode_limit_2"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/inode_limit_2",
                                                                      .inode_limit = 2,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "root_dir_mode_0000"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "root_dir_mode_0000",
                                                                      .root_dir_mode = 0000,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "root_dir_mode_0123"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "root_dir_mode_0123",
                                                                      .root_dir_mode = 0123,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "read_only_true"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/read_only_true",
                                                                      .read_only = true,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/"
                                                                              "read_only_false"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/"
                                                                              "read_only_false",
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/no_exec_true"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/no_exec_true",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          nullopt,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                      .no_exec = true,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/tmp/no_exec_false"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/tmp/no_exec_false",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          nullopt,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                      .no_exec = false,
                                                                  },
                                                              }
                                                          },
                                                  },
                                          },
                                  }
                              )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, mount_tmpfs_invalid_root_dir_mode) {
    auto& sc = get_sc();
    ASSERT_THAT(
        [&] {
            (void)sc.send_request(
                {{tester_executable_path}},
                {
                    .stderr_fd = STDERR_FILENO,
                    .linux_namespaces =
                        {
                            .mount =
                                {
                                    .operations = {{
                                        MountTmpfs{.path = "/tmp", .root_dir_mode = 01000},
                                    }},
                                },
                        },
                }
            );
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("MountTmpfs: invalid root_dir_mode: 512")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, mount_proc) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            FileDescriptor{tester_executable_path.data(), O_RDONLY},
            {{tester_executable_path, "mount_proc"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/tmp",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/tmp/read_only_true"},
                                    MountProc{
                                        .path = "/tmp/read_only_true",
                                        .read_only = true,
                                    },
                                    CreateDir{.path = "/tmp/read_only_false"},
                                    MountProc{
                                        .path = "/tmp/read_only_false",
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/tmp/no_exec_true"},
                                    MountProc{
                                        .path = "/tmp/no_exec_true",
                                        .no_exec = true,
                                    },
                                    CreateDir{.path = "/tmp/no_exec_false"},
                                    MountProc{
                                        .path = "/tmp/no_exec_false",
                                        .no_exec = false,
                                    },
                                }},
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, bind_mount) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(sc.await_result(sc.send_request(FileDescriptor{tester_executable_path.data(), O_RDONLY},
        {{tester_executable_path, "bind_mount"}},
        {
            .stderr_fd = STDERR_FILENO,
            .linux_namespaces = {
                .mount = {
                    .operations = {{
                        MountTmpfs{
                            .path = "/tmp",
                            .inode_limit = nullopt,
                            .read_only = false,
                        },
                        CreateDir{.path = "/tmp/tree"},
                        MountTmpfs{
                            .path = "/tmp/tree",
                            .max_total_size_of_files_in_bytes = nullopt,
                            .inode_limit = nullopt,
                            .read_only = false,
                            .no_exec = false,
                        },
                        CreateFile{.path = "/tmp/tree/file"},
                        CreateDir{.path = "/tmp/tree/dir"},
                        MountTmpfs{
                            .path = "/tmp/tree/dir",
                            .max_total_size_of_files_in_bytes = nullopt,
                            .inode_limit = nullopt,
                            .read_only = false,
                            .no_exec = false,
                        },
                        CreateFile{.path = "/tmp/tree/dir/file"},
                        CreateDir{.path = "/tmp/tree/dir/dir"},
                        CreateDir{.path = "/tmp/recursive_true"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/recursive_true",
                            .recursive = true,
                        },
                        CreateDir{.path = "/tmp/recursive_false"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/recursive_false",
                            .recursive = false,
                        },
                        CreateDir{.path = "/tmp/read_only_true_recursive_true"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/read_only_true_recursive_true",
                            .recursive = true,
                            .read_only = true,
                        },
                        CreateDir{.path = "/tmp/read_only_true_recursive_false"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/read_only_true_recursive_false",
                            .recursive = false,
                            .read_only = true,
                        },
                        CreateDir{.path = "/tmp/read_only_false_recursive_true"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/read_only_false_recursive_true",
                            .recursive = true,
                            .read_only = false,
                        },
                        CreateDir{.path = "/tmp/read_only_false_recursive_false"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/read_only_false_recursive_false",
                            .recursive = false,
                            .read_only = false,
                        },
                        CreateDir{.path = "/tmp/no_exec_true_recursive_true"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/no_exec_true_recursive_true",
                            .recursive = true,
                            .read_only = false,
                            .no_exec = true,
                        },
                        CreateDir{.path = "/tmp/no_exec_true_recursive_false"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/no_exec_true_recursive_false",
                            .recursive = false,
                            .read_only = false,
                            .no_exec = true,
                        },
                        CreateDir{.path = "/tmp/no_exec_false_recursive_true"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/no_exec_false_recursive_true",
                            .recursive = true,
                            .read_only = false,
                            .no_exec = false,
                        },
                        CreateDir{.path = "/tmp/no_exec_false_recursive_false"},
                        BindMount{
                            .source = "/tmp/tree",
                            .dest = "/tmp/no_exec_false_recursive_false",
                            .recursive = false,
                            .read_only = false,
                            .no_exec = false,
                        },
                    }},
                },
            },
        }
    )), CLD_EXITED, 0);
}

// NOLINTNEXTLINE
TEST(sandbox, create_dir) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            FileDescriptor{tester_executable_path.data(), O_RDONLY},
            {{tester_executable_path, "create_dir"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/tmp",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{
                                        .path = "/tmp/dir_mode_0000",
                                        .mode = 0000,
                                    },
                                    CreateDir{
                                        .path = "/tmp/dir_mode_0314",
                                        .mode = 0314,
                                    },
                                }},
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, create_dir_invalid_mode) {
    auto& sc = get_sc();
    ASSERT_THAT(
        [&] {
            (void)sc.send_request(
                {{tester_executable_path}},
                {
                    .stderr_fd = STDERR_FILENO,
                    .linux_namespaces =
                        {
                            .mount =
                                {
                                    .operations = {{
                                        MountTmpfs{
                                            .path = "/tmp",
                                            .inode_limit = nullopt,
                                            .read_only = false,
                                        },
                                        CreateDir{.path = "/tmp/dir", .mode = 01000},
                                    }},
                                },
                        },
                }
            );
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("CreateDir: invalid mode: 512")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, create_existing_dir_fails) {
    auto& sc = get_sc();
    ASSERT_RESULT_ERROR(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/tmp",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/tmp/dir"},
                                    CreateDir{.path = "/tmp/dir"},
                                }},
                            },
                    },
            }
        )),
        "pid1: mkdir(\"/tmp/dir\") - File exists (os error 17)"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, create_file) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            FileDescriptor{tester_executable_path.data(), O_RDONLY},
            {{tester_executable_path, "create_file"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/tmp",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateFile{
                                        .path = "/tmp/file_mode_0000",
                                        .mode = 0000,
                                    },
                                    CreateFile{
                                        .path = "/tmp/file_mode_0314",
                                        .mode = 0314,
                                    },
                                }},
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}

// NOLINTNEXTLINE
TEST(sandbox, create_file_invalid_mode) {
    auto& sc = get_sc();
    ASSERT_THAT(
        [&] {
            (void)sc.send_request(
                {{tester_executable_path}},
                {
                    .stderr_fd = STDERR_FILENO,
                    .linux_namespaces =
                        {
                            .mount =
                                {
                                    .operations = {{
                                        MountTmpfs{
                                            .path = "/tmp",
                                            .inode_limit = nullopt,
                                            .read_only = false,
                                        },
                                        CreateFile{.path = "/tmp/file", .mode = 01000},
                                    }},
                                },
                        },
                }
            );
        },
        testing::ThrowsMessage<std::runtime_error>(
            testing::StartsWith("CreateFile: invalid mode: 512")
        )
    );
}

// NOLINTNEXTLINE
TEST(sandbox, creating_existing_file_fails) {
    auto& sc = get_sc();
    ASSERT_RESULT_ERROR(
        sc.await_result(sc.send_request(
            {{tester_executable_path}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/tmp",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateFile{.path = "/tmp/file"},
                                    CreateFile{.path = "/tmp/file"},
                                }},
                            },
                    },
            }
        )),
        "pid1: open(\"/tmp/file\", O_CREAT | O_EXCL) - File exists (os error 17)"
    );
}

// NOLINTNEXTLINE
TEST(sandbox, tracee_cannot_umount_mounts) {
    auto& sc = get_sc();
    ASSERT_RESULT_OK(
        sc.await_result(sc.send_request(
            FileDescriptor{tester_executable_path.data(), O_RDONLY},
            {{tester_executable_path, "tracee_cannot_umount_mounts"}},
            {
                .stderr_fd = STDERR_FILENO,
                .linux_namespaces =
                    {
                        .mount =
                            {
                                .operations = {{
                                    MountTmpfs{
                                        .path = "/tmp",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/tmp/tmp"},
                                    MountTmpfs{.path = "/tmp/tmp"},
                                    CreateDir{.path = "/tmp/proc"},
                                    MountProc{.path = "/tmp/proc"},
                                    CreateDir{.path = "/tmp/bind"},
                                    BindMount{
                                        .source = "/tmp", .dest = "/tmp/bind", .recursive = false
                                    },
                                    CreateDir{.path = "/tmp/bind_recursive"},
                                    BindMount{
                                        .source = "/tmp",
                                        .dest = "/tmp/bind_recursive",
                                        .recursive = true
                                    },
                                }},
                            },
                    },
            }
        )),
        CLD_EXITED,
        0
    );
}
