#include <simlib/slice.hh>
#include <simlib/string_traits.hh>

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <set>
#include <simlib/concat_tostr.hh>
#include <simlib/directory.hh>
#include <simlib/file_path.hh>
#include <simlib/logger.hh>
#include <simlib/merge.hh>
#include <simlib/overloaded.hh>
#include <simlib/sandbox/sandbox.hh>
#include <simlib/throw_assert.hh>
#include <simlib/to_arg_seq.hh>
#include <simlib/utilities.hh>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <variant>
#include <vector>

using std::string;
using std::string_view;
using std::vector;

struct Dir {
    string path;
};

struct File {
    string path;
};

using Entry = std::variant<Dir, File>;

struct CreateDir {
    string path;
};

struct CreateFile {
    string path;
};

struct BindMount {
    string source;
    string dest;
    bool read_only;
};

using MountOperation = std::variant<CreateDir, CreateFile, BindMount>;

constexpr string_view NEW_ROOT_PATH = "/..";

bool runs_with(
    sandbox::SupervisorConnection& sb,
    const vector<string_view>& sargv,
    const vector<string_view>& env,
    const vector<MountOperation>& mount_operations
) {
    auto res = sb.await_result(sb.send_request(
        sargv,
        {
            .stdin_fd = std::nullopt,
            .stdout_fd = std::nullopt,
            .stderr_fd = std::nullopt,
            .env = env,
            .linux_namespaces =
                {
                    .mount =
                        {
                            .operations =
                                [&] {
                                    using Mount = sandbox::RequestOptions::LinuxNamespaces::Mount;
                                    std::vector<
                                        sandbox::RequestOptions::LinuxNamespaces::Mount::Operation>
                                        ops = {
                                            Mount::MountTmpfs{
                                                .path = "/",
                                                .max_total_size_of_files_in_bytes = 1 << 30,
                                                .inode_limit = 1'000'000,
                                                .read_only = false,
                                            },
                                        };
                                    for (auto const& mo : mount_operations) {
                                        std::visit(
                                            overloaded{
                                                [&](const CreateDir& cd) {
                                                    ops.emplace_back(Mount::CreateDir{
                                                        .path = cd.path});
                                                },
                                                [&](const CreateFile& cf) {
                                                    ops.emplace_back(Mount::CreateFile{
                                                        .path = cf.path});
                                                },
                                                [&](const BindMount& bm) {
                                                    ops.emplace_back(Mount::BindMount{
                                                        .source = bm.source,
                                                        .dest = bm.dest,
                                                        .recursive = true,
                                                        .read_only = bm.read_only,
                                                        .no_exec = false,
                                                        .symlink_nofollow = true,
                                                    });
                                                },
                                            },
                                            mo
                                        );
                                    }
                                    return ops;
                                }(),
                            .new_root_mount_path = NEW_ROOT_PATH,
                        },
                },
        }
    ));
    return std::visit(
        overloaded{
            [](const sandbox::result::Ok& res_ok) {
                return res_ok.si == sandbox::Si{.code = CLD_EXITED, .status = 0};
            },
            [](const sandbox::result::Error& /*res_err*/) { return false; },
        },
        res
    );
}

void append_entry(vector<MountOperation>& mount_operations, const Entry& entry, bool read_only) {
    std::visit(
        overloaded{
            [&](const File& file) {
                mount_operations.emplace_back(CreateFile{
                    .path = concat_tostr(NEW_ROOT_PATH, file.path)});
                mount_operations.emplace_back(BindMount{
                    .source = file.path,
                    .dest = concat_tostr(NEW_ROOT_PATH, file.path),
                    .read_only = read_only,
                });
            },
            [&](const Dir& dir) {
                mount_operations.emplace_back(CreateDir{
                    .path = concat_tostr(NEW_ROOT_PATH, dir.path)});
                mount_operations.emplace_back(BindMount{
                    .source = dir.path,
                    .dest = concat_tostr(NEW_ROOT_PATH, dir.path),
                    .read_only = read_only,
                });
            },
        },
        entry
    );
}

bool runs_with(
    sandbox::SupervisorConnection& sb,
    const vector<string_view>& sargv,
    const vector<string_view>& env,
    vector<MountOperation>& mount_operations,
    Slice<const Entry> entries
) {
    auto saved_size = mount_operations.size();
    for (const auto& entry : entries) {
        append_entry(mount_operations, entry, false);
    }
    auto res = runs_with(sb, sargv, env, mount_operations);
    mount_operations.resize(saved_size);
    return res;
}

void filter_entries(
    sandbox::SupervisorConnection& sb,
    const vector<string_view>& sargv,
    const vector<string_view>& env,
    vector<MountOperation>& mount_operations,
    vector<Entry>& entries,
    const vector<string_view>& excluded_from_going_deeper_paths
) {
    assert(std::is_sorted(excluded_from_going_deeper_paths.begin(), excluded_from_going_deeper_paths.end()));
    while (!entries.empty()) {
        assert(runs_with(sb, sargv, env, mount_operations, entries));
        size_t entries_len = entries.size();
        size_t k = 1;
        while (k < entries_len &&
               runs_with(sb, sargv, env, mount_operations, Slice{entries.data(), entries_len - k}))
        {
            entries_len -= k;
            k <<= 1;
        }
        while (k > 0) {
            if (k <= entries_len && runs_with(sb, sargv, env, mount_operations, Slice{entries.data(), entries_len - k}))
            {
                entries_len -= k;
            }
            k >>= 1;
        }
        if (entries_len == 0) {
            break; // All needed entries are in mount_operations
        }
        --entries_len;
        auto found_entry = std::move(entries[entries_len]);
        entries.resize(entries_len);

        std::visit(overloaded{
            [&](const File& file) {
                mount_operations.emplace_back(CreateFile{
                    .path = concat_tostr(NEW_ROOT_PATH, file.path)});
                mount_operations.emplace_back(BindMount{
                    .source = file.path,
                    .dest = concat_tostr(NEW_ROOT_PATH, file.path),
                    .read_only = true,
                });
                if (runs_with(sb, sargv, env, mount_operations, Slice{entries.data(), entries_len})) {
                    stdlog("RO ", file.path);
                } else {
                    std::get<BindMount>(mount_operations.back()).read_only = false;
                    stdlog("RW ", file.path);
                }
            },
            [&](const Dir& dir) {
                mount_operations.emplace_back(CreateDir{
                    .path = concat_tostr(NEW_ROOT_PATH, dir.path)});
                mount_operations.emplace_back(BindMount{
                    .source = dir.path,
                    .dest = concat_tostr(NEW_ROOT_PATH, dir.path),
                    .read_only = true,
                });

                bool read_only = runs_with(sb, sargv, env, mount_operations, Slice{entries.data(), entries_len});
                stdlog(read_only ? "RO" : "RW", ' ', dir.path);

                if (binary_search(excluded_from_going_deeper_paths.begin(), excluded_from_going_deeper_paths.end(), dir.path)) {
                    std::get<BindMount>(mount_operations.back()).read_only = read_only;
                } else {
                    mount_operations.pop_back();

                    // Recurse into directory
                    for_each_dir_component(dir.path, [&](dirent* file) {
                        auto path = concat_tostr(dir.path, "/", file->d_name);
                        struct stat64 st;
                        throw_assert(fstatat64(AT_FDCWD, path.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0);
                        if (S_ISDIR(st.st_mode)) {
                            entries.emplace_back(Dir{
                                .path = std::move(path),
                            });
                        } else {
                            entries.emplace_back(File{
                                .path = std::move(path),
                            });
                        }
                    });
                }
            },
        }, found_entry);
    }
}

int main(int argc, char** argv) {
    auto sb = sandbox::spawn_supervisor();
    std::vector<string_view> excluded_from_going_deeper_paths;
    vector<string_view> sargv;
    sargv.reserve(argc - 1);
    bool non_exclude_arg_found = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (!non_exclude_arg_found && !arg.empty() && arg[0] == '-') {
            arg.remove_prefix(1);
            excluded_from_going_deeper_paths.emplace_back(arg);
        } else {
            sargv.emplace_back(arg);
            non_exclude_arg_found = true;
        }
    }
    if (sargv.empty()) {
        (void)fprintf(
            stderr,
            "Usage: %s [-PATH_EXCLUDED_FROM_GOING_DEEPER]... EXECUTABLE [ARGS]...\n",
            argv[0]
        );
        return 1;
    }
    vector<string_view> env;
    for (auto e = environ; *e; ++e) {
        env.emplace_back(*e);
    }

    std::vector<Entry> root_dir_entries;
    for_each_dir_component("/", [&](dirent* file) {
        auto path = concat_tostr("/", file->d_name);
        struct stat64 st;
        throw_assert(fstatat64(AT_FDCWD, path.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0);
        if (S_ISDIR(st.st_mode)) {
            root_dir_entries.emplace_back(Dir{.path = std::move(path)});
        } else {
            root_dir_entries.emplace_back(File{.path = std::move(path)});
        }
    });

    std::vector<MountOperation> mount_operations;
    if (!runs_with(sb, sargv, env, mount_operations, root_dir_entries)) {
        (void)fprintf(stderr, "Error: the command does not run with the whole / mounted\n");
        return 1;
    }

    stdlog.use(stdout);
    stdlog.label(false);

    sort(excluded_from_going_deeper_paths.begin(), excluded_from_going_deeper_paths.end());
    filter_entries(sb, sargv, env, mount_operations, root_dir_entries, excluded_from_going_deeper_paths);

    return 0;
}
