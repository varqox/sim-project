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
};

using MountOperation = std::variant<CreateDir, CreateFile, BindMount>;

int main(int argc, char** argv) {
    auto sb = sandbox::spawn_supervisor();
    std::set<string_view> excluded_from_going_deeper_paths;
    vector<string_view> sargv;
    sargv.reserve(argc - 1);
    bool non_exclude_arg_found = false;
    for (auto arg : to_arg_seq(argc, argv)) {
        if (!non_exclude_arg_found && !arg.empty() && arg[0] == '-') {
            arg.remove_prefix(1);
            excluded_from_going_deeper_paths.emplace(arg);
        } else {
            sargv.emplace_back(arg);
            non_exclude_arg_found = true;
        }
    }
    if (sargv.empty()) {
        (void)fprintf(
            stderr, "Usage: %s [-PATH_EXCLUDED_FROM_GOING_DEEPER]... EXECUTABLE [ARGS]...", argv[0]
        );
        return 1;
    }
    vector<string_view> env;
    for (auto e = environ; *e; ++e) {
        env.emplace_back(*e);
    }

    auto runs_with = [&](const vector<MountOperation>& mount_operations) {
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
                                        using Mount =
                                            sandbox::RequestOptions::LinuxNamespaces::Mount;
                                        std::vector<sandbox::RequestOptions::LinuxNamespaces::
                                                        Mount::Operation>
                                            ops = {
                                                Mount::MountTmpfs{
                                                    .path = "/",
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
                                                    [&](const CreateFile& cd) {
                                                        ops.emplace_back(Mount::CreateFile{
                                                            .path = cd.path});
                                                    },
                                                    [&](const BindMount& cd) {
                                                        ops.emplace_back(Mount::BindMount{
                                                            .source = cd.source,
                                                            .dest = cd.dest,
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
                                .new_root_mount_path = "/..",
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
    };

    auto mount_operations = vector<MountOperation>{};

    std::vector<Entry> entries_to_check;
    auto append_entries_of = [&](FilePath dir, FilePath prefix) {
        for_each_dir_component(dir, [&](dirent* file) {
            auto path = concat_tostr(prefix, "/", file->d_name);
            struct stat64 st;
            throw_assert(fstatat64(AT_FDCWD, path.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0);
            if (S_ISDIR(st.st_mode)) {
                entries_to_check.emplace_back(Dir{.path = std::move(path)});
            } else {
                entries_to_check.emplace_back(File{.path = std::move(path)});
            }
        });
    };
    append_entries_of("/", "");

    auto runs_with_all_entries = [&] {
        auto saved_size = mount_operations.size();
        for (const auto& entry : entries_to_check) {
            std::visit(
                overloaded{
                    [&mount_operations](const Dir& dir) {
                        mount_operations.emplace_back(CreateDir{
                            .path = concat_tostr("/..", dir.path),
                        });
                        mount_operations.emplace_back(BindMount{
                            .source = dir.path,
                            .dest = concat_tostr("/..", dir.path),
                        });
                    },
                    [&mount_operations](const File& file) {
                        mount_operations.emplace_back(CreateFile{
                            .path = concat_tostr("/..", file.path),
                        });
                        mount_operations.emplace_back(BindMount{
                            .source = file.path,
                            .dest = concat_tostr("/..", file.path),
                        });
                    },
                },
                entry
            );
        }
        auto res = runs_with(mount_operations);
        mount_operations.resize(saved_size);
        return res;
    };

    stdlog.use(stdout);
    stdlog.label(false);

    size_t checked_entries = 0;
    size_t total_entries = entries_to_check.size();
    bool logging_progress = isatty(STDOUT_FILENO);
    auto log_progress = [&] {
        if (logging_progress) {
            stdlog("\033[2K\033[GProgress: ", checked_entries, "/", total_entries).flush_no_nl();
        }
    };

    while (!entries_to_check.empty()) {
        log_progress();
        auto entry = entries_to_check.back();
        entries_to_check.pop_back();
        ++checked_entries;
        if (runs_with_all_entries()) {
            continue;
        }

        if (logging_progress) {
            stdlog("\033[2K\033[G").flush_no_nl();
        }
        std::visit(
            overloaded{
                [&](const Dir& dir) {
                    stdlog(dir.path, '/');
                    mount_operations.emplace_back(CreateDir{
                        .path = concat_tostr("/..", dir.path),
                    });
                    if (excluded_from_going_deeper_paths.count(dir.path) != 0) {
                        mount_operations.emplace_back(BindMount{
                            .source = dir.path,
                            .dest = concat_tostr("/..", dir.path),
                        });
                    } else {
                        auto saved_size = entries_to_check.size();
                        append_entries_of(dir.path, dir.path);
                        total_entries += entries_to_check.size() - saved_size;
                    }
                },
                [&](const File& file) {
                    stdlog(file.path);
                    mount_operations.emplace_back(CreateFile{
                        .path = concat_tostr("/..", file.path),
                    });
                    mount_operations.emplace_back(BindMount{
                        .source = file.path,
                        .dest = concat_tostr("/..", file.path),
                    });
                },
            },
            entry
        );
    }
    log_progress();
    if (logging_progress) {
        stdlog("");
    }
}
