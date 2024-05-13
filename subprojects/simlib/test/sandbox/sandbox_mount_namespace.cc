#include "../gtest_with_tester.hh"
#include "assert_result.hh"

#include <fcntl.h>
#include <gmock/gmock.h>
#include <optional>
#include <simlib/file_descriptor.hh>
#include <simlib/sandbox/sandbox.hh>
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
                                        .path = "/sys",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/sys/usr"},
                                    BindMount{
                                        .source = "/usr",
                                        .dest = "/sys/usr",
                                        .no_exec = false,
                                    },
                                    CreateDir{.path = "/sys/lib"},
                                    BindMount{
                                        .source = "/lib",
                                        .dest = "/sys/lib",
                                        .no_exec = false,
                                    },
                                    CreateDir{.path = "/sys/lib64"},
                                    BindMount{
                                        .source = "/lib64",
                                        .dest = "/sys/lib64",
                                        .no_exec = false,
                                    },
                                }},
                                .new_root_mount_path = "/sys",
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
                                                                      .path = "/dev",
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_nullopt"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_nullopt",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          std::nullopt,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_0"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_0",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          0,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_1"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_1",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          1,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_32768"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "max_total_size_of_"
                                                                              "files_32768",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          32768,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "inode_limit_"
                                                                              "nullopt"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "inode_limit_nullopt",
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/inode_limit_0"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/inode_limit_0",
                                                                      .inode_limit = 0,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/inode_limit_1"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/inode_limit_1",
                                                                      .inode_limit = 1,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/inode_limit_2"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/inode_limit_2",
                                                                      .inode_limit = 2,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "root_dir_mode_0000"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "root_dir_mode_0000",
                                                                      .root_dir_mode = 0000,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "root_dir_mode_0123"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "root_dir_mode_0123",
                                                                      .root_dir_mode = 0123,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "read_only_true"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/read_only_true",
                                                                      .read_only = true,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/"
                                                                              "read_only_false"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/"
                                                                              "read_only_false",
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/no_exec_true"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/no_exec_true",
                                                                      .max_total_size_of_files_in_bytes =
                                                                          nullopt,
                                                                      .inode_limit = nullopt,
                                                                      .read_only = false,
                                                                      .no_exec = true,
                                                                  },
                                                                  CreateDir{
                                                                      .path = "/dev/no_exec_false"
                                                                  },
                                                                  MountTmpfs{
                                                                      .path = "/dev/no_exec_false",
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
                                        MountTmpfs{.path = "/dev", .root_dir_mode = 01000},
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
                                        .path = "/dev",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/dev/read_only_true"},
                                    MountProc{
                                        .path = "/dev/read_only_true",
                                        .read_only = true,
                                    },
                                    CreateDir{.path = "/dev/read_only_false"},
                                    MountProc{
                                        .path = "/dev/read_only_false",
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/dev/no_exec_true"},
                                    MountProc{
                                        .path = "/dev/no_exec_true",
                                        .no_exec = true,
                                    },
                                    CreateDir{.path = "/dev/no_exec_false"},
                                    MountProc{
                                        .path = "/dev/no_exec_false",
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
                            .path = "/dev",
                            .inode_limit = nullopt,
                            .read_only = false,
                        },
                        CreateDir{.path = "/dev/tree"},
                        MountTmpfs{
                            .path = "/dev/tree",
                            .max_total_size_of_files_in_bytes = nullopt,
                            .inode_limit = nullopt,
                            .read_only = false,
                            .no_exec = false,
                        },
                        CreateFile{.path = "/dev/tree/file"},
                        CreateDir{.path = "/dev/tree/dir"},
                        MountTmpfs{
                            .path = "/dev/tree/dir",
                            .max_total_size_of_files_in_bytes = nullopt,
                            .inode_limit = nullopt,
                            .read_only = false,
                            .no_exec = false,
                        },
                        CreateFile{.path = "/dev/tree/dir/file"},
                        CreateDir{.path = "/dev/tree/dir/dir"},
                        CreateDir{.path = "/dev/recursive_true"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/recursive_true",
                            .recursive = true,
                        },
                        CreateDir{.path = "/dev/recursive_false"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/recursive_false",
                            .recursive = false,
                        },
                        CreateDir{.path = "/dev/read_only_true_recursive_true"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/read_only_true_recursive_true",
                            .recursive = true,
                            .read_only = true,
                        },
                        CreateDir{.path = "/dev/read_only_true_recursive_false"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/read_only_true_recursive_false",
                            .recursive = false,
                            .read_only = true,
                        },
                        CreateDir{.path = "/dev/read_only_false_recursive_true"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/read_only_false_recursive_true",
                            .recursive = true,
                            .read_only = false,
                        },
                        CreateDir{.path = "/dev/read_only_false_recursive_false"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/read_only_false_recursive_false",
                            .recursive = false,
                            .read_only = false,
                        },
                        CreateDir{.path = "/dev/no_exec_true_recursive_true"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/no_exec_true_recursive_true",
                            .recursive = true,
                            .read_only = false,
                            .no_exec = true,
                        },
                        CreateDir{.path = "/dev/no_exec_true_recursive_false"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/no_exec_true_recursive_false",
                            .recursive = false,
                            .read_only = false,
                            .no_exec = true,
                        },
                        CreateDir{.path = "/dev/no_exec_false_recursive_true"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/no_exec_false_recursive_true",
                            .recursive = true,
                            .read_only = false,
                            .no_exec = false,
                        },
                        CreateDir{.path = "/dev/no_exec_false_recursive_false"},
                        BindMount{
                            .source = "/dev/tree",
                            .dest = "/dev/no_exec_false_recursive_false",
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
                                        .path = "/dev",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{
                                        .path = "/dev/dir_mode_0000",
                                        .mode = 0000,
                                    },
                                    CreateDir{
                                        .path = "/dev/dir_mode_0314",
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
                                            .path = "/dev",
                                            .inode_limit = nullopt,
                                            .read_only = false,
                                        },
                                        CreateDir{.path = "/dev/dir", .mode = 01000},
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
                                        .path = "/dev",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/dev/dir"},
                                    CreateDir{.path = "/dev/dir"},
                                }},
                            },
                    },
            }
        )),
        "pid1: mkdir(\"/dev/dir\") - File exists (os error 17)"
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
                                        .path = "/dev",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateFile{
                                        .path = "/dev/file_mode_0000",
                                        .mode = 0000,
                                    },
                                    CreateFile{
                                        .path = "/dev/file_mode_0314",
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
                                            .path = "/dev",
                                            .inode_limit = nullopt,
                                            .read_only = false,
                                        },
                                        CreateFile{.path = "/dev/file", .mode = 01000},
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
                                        .path = "/dev",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateFile{.path = "/dev/file"},
                                    CreateFile{.path = "/dev/file"},
                                }},
                            },
                    },
            }
        )),
        "pid1: open(\"/dev/file\", O_CREAT | O_EXCL) - File exists (os error 17)"
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
                                        .path = "/dev",
                                        .inode_limit = nullopt,
                                        .read_only = false,
                                    },
                                    CreateDir{.path = "/dev/tmp"},
                                    MountTmpfs{.path = "/dev/tmp"},
                                    CreateDir{.path = "/dev/proc"},
                                    MountProc{.path = "/dev/proc"},
                                    CreateDir{.path = "/dev/bind"},
                                    BindMount{
                                        .source = "/dev", .dest = "/dev/bind", .recursive = false
                                    },
                                    CreateDir{.path = "/dev/bind_recursive"},
                                    BindMount{
                                        .source = "/dev",
                                        .dest = "/dev/bind_recursive",
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
