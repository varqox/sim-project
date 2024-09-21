#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <sim/internal_files/internal_file.hh>
#include <sim/internal_files/old_internal_file.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/directory.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_info.hh>
#include <simlib/logger.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/spawner.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <simlib/to_arg_seq.hh>
#include <simlib/working_directory.hh>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

using std::string;
using std::string_view;
using std::vector;

namespace {

constexpr inline auto DEFAULT_BACKUP_REPOSITORY_PATH = string_view{"backup.borg"};
constexpr inline auto SIM_DIR_RELATIVE_TO_EXECUTABLE = string_view{".."};

template <class... Args>
void die_with_error(Args&&... args) {
    bool stderr_is_tty = isatty(STDERR_FILENO);
    if (stderr_is_tty) {
        std::cerr << "\033[1;31m";
    }
    std::cerr << "error: ";
    if (stderr_is_tty) {
        std::cerr << "\033[m\033[1m";
    }
    (std::cerr << ... << std::forward<Args>(args));
    if (stderr_is_tty) {
        std::cerr << "\033[m";
    }
    std::cerr << std::endl;
    _exit(1);
}

void help(const char* program_name) {
    if (!program_name) {
        program_name = "backup";
    }

    printf("Usage: %s <command> [<args>]\n", program_name);
    puts("Commands:");
    puts("   list [--from <PATH_TO_REPOSITORY>]");
    puts("            List all snapshots. Uses backup.borg from the current instance if");
    puts("            PATH_TO_REPOSITORY is not specified.");
    puts("   save [--to <PATH_TO_REPOSITORY>]");
    puts("            Save snapshot to the repository. Uses backup.borg from the current");
    puts("            instance if PATH_TO_REPOSITORY is not specified.");
    puts("   restore <SNAPSHOT_NAME> [--from <PATH_TO_REPOSITORY>]");
    puts("            Restore the specified snapshot. Use \"list\" command to get snapshot");
    puts("            names. Uses backup.borg from the current instance if");
    puts("            PATH_TO_REPOSITORY is not specified.");
    puts("");
    puts("            WARNING: This operation is destructive -- it will overwrite files");
    puts("            present in the snapshot and delete files absent in the snapshot.");
    puts("   help     Prints this message");
}

// Returns path to repository valid after chdir()
string parse_repository_path_and_chdir_to_sim_dir(
    ArgSeq::Iter begin, ArgSeq::Iter end, string_view arg_preceding_path_to_repository
) {
    if (begin == end) {
        chdir_relative_to_executable_dirpath(SIM_DIR_RELATIVE_TO_EXECUTABLE);
        return string{DEFAULT_BACKUP_REPOSITORY_PATH};
    }

    auto first_arg = *begin++;
    if (first_arg != arg_preceding_path_to_repository) {
        die_with_error("unexpected argument: ", first_arg);
    }

    if (begin == end) {
        die_with_error("missing argument <PATH_TO_REPOSITORY>");
    }
    auto path_to_repository = *begin++;
    if (begin != end) {
        die_with_error("unexpected argument: ", *begin);
    }

    if (has_prefix(path_to_repository, "/")) {
        chdir_relative_to_executable_dirpath(SIM_DIR_RELATIVE_TO_EXECUTABLE);
        return string{path_to_repository};
    }
    auto pwd = get_cwd().to_string();
    chdir_relative_to_executable_dirpath(SIM_DIR_RELATIVE_TO_EXECUTABLE);
    return concat_tostr(pwd, path_to_repository);
}

void run_command(
    vector<string> argv, FileDescriptor* stdin_fd = nullptr, FileDescriptor* stdout_fd = nullptr
) {
    throw_assert(!argv.empty());
    auto options = Spawner::Options{};
    if (stdin_fd) {
        options.new_stdin_fd = *stdin_fd;
    }
    if (stdout_fd) {
        options.new_stdout_fd = *stdout_fd;
    }
    auto es = Spawner::run(argv[0], argv, options);
    if (es.si.code != CLD_EXITED || es.si.status != 0) {
        die_with_error(argv[0], " failed: ", es.message);
    }
}

struct DbConfig {
    string user;
    string password;
    string db;
    string host;
};

DbConfig load_db_config() {
    ConfigFile cf;
    cf.add_vars("user", "password", "db", "host");
    cf.load_config_from_file(".db.config");
    const auto& user = cf.get_var("user");
    throw_assert(user.is_set());
    const auto& password = cf.get_var("password");
    throw_assert(password.is_set());
    const auto& db = cf.get_var("db");
    throw_assert(db.is_set());
    const auto& host = cf.get_var("host");
    throw_assert(host.is_set());
    return {
        .user = user.as_string(),
        .password = password.as_string(),
        .db = db.as_string(),
        .host = host.as_string(),
    };
}

void list(ArgSeq::Iter begin, ArgSeq::Iter end) {
    auto repository_path = parse_repository_path_and_chdir_to_sim_dir(begin, end, "--from");
    run_command({"/usr/bin/borg", "list", "--short", repository_path});
}

void save(ArgSeq::Iter begin, ArgSeq::Iter end) {
    auto repository_path = parse_repository_path_and_chdir_to_sim_dir(begin, end, "--to");

    // Remove temporary internal files that were not removed e.g. because of web/job server crash
    stdlog("Removing orphaned temporary internal files...");
    {
        using std::chrono::system_clock;
        using namespace std::chrono_literals;

        auto mysql = sim::mysql::Connection::from_credential_file(".db.config");
        auto transaction = mysql.start_repeatable_read_transaction();

        // Remove internal files that do not have an entry in internal_files
        sim::PackageContents internal_files;
        internal_files.load_from_directory(sim::internal_files::dir);

        std::set<StringView> orphaned_files;
        internal_files.for_each_with_prefix("", [&](StringView path) {
            orphaned_files.emplace(path);
        });

        auto stmt = mysql.execute(sim::sql::Select("id").from("internal_files"));
        decltype(sim::internal_files::InternalFile::id) internal_file_id;
        stmt.res_bind(internal_file_id);
        while (stmt.next()) {
            auto it = orphaned_files.find(from_unsafe{to_string(internal_file_id)});
            throw_assert(it != orphaned_files.end());
            orphaned_files.erase(it);
        }

        // Remove orphaned files that are older than 2h (not to delete files that are just created
        // but not committed)
        for (const auto& filename : orphaned_files) {
            struct stat64 st;
            auto file_path = concat_tostr(sim::internal_files::dir, filename);
            if (stat64(file_path.c_str(), &st)) {
                if (errno == ENOENT) {
                    continue; // File disappeared, probably it was temporary and the transaction
                              // rolled back, so the file was removed.
                }
                THROW("stat64()");
            }

            if (system_clock::now() - get_modification_time(st) > std::chrono::hours(2)) {
                stdlog("Deleting: ", file_path);
                if (unlink(file_path.c_str())) {
                    THROW("unlink()");
                }
            }
        }
    }
    stdlog("... done.");

    stdlog("Dumping the database...");
    {
        auto db_config = load_db_config();
        run_command({
            "/usr/bin/mariadb-dump",
            concat_tostr("--user=", db_config.user),
            concat_tostr("--password=", db_config.password),
            concat_tostr("--host=", db_config.host),
            "--single-transaction",
            "--result-file=dump.sql",
            db_config.db,
        });
    }
    stdlog("... done.");

    if (chmod("dump.sql", S_0600)) {
        THROW("chmod()", errmsg());
    }

    if (!path_exists(repository_path)) {
        run_command({"/usr/bin/borg", "init", "--encryption", "none", repository_path});
    }
    run_command({
        "/usr/bin/borg",
        "create",
        "--verbose",
        "--stats",
        "--progress",
        repository_path + "::{now}",
        ".db.config",
        "bin",
        "dump.sql",
        "internal_files",
        "lib",
        "logs",
        "manage",
        "sim.conf",
        "static",
    });
}

void restore(ArgSeq::Iter begin, ArgSeq::Iter end) {
    if (begin == end) {
        die_with_error("missing argument <SNAPSHOT_NAME>");
    }
    auto snapshot_name = *begin++;

    auto repository_path = parse_repository_path_and_chdir_to_sim_dir(begin, end, "--from");
    // Stop Sim so it doesn't run on the messed up (during restoration) database and files
    stdlog("Stopping Sim...");
    run_command({"./manage", "stop"});
    stdlog("... done.");

    // Restore all files except modifications to .db.config
    auto db_config_contents = get_file_contents(".db.config");
    stdlog("Restoring files (including database dump)...");
    auto borg_archive = concat_tostr(repository_path, "::", snapshot_name);
    run_command({
        "/usr/bin/borg",
        "extract",
        "--progress",
        borg_archive,
    });
    put_file_contents(".db.config", db_config_contents);
    stdlog("... done.");

    stdlog("Removing files absent in the backup...");
    // NOLINTNEXTLINE(android-cloexec-memfd-create)
    auto out_fd = FileDescriptor{memfd_create("backup file list", 0)};
    if (!out_fd.is_open()) {
        THROW("memfd_create()");
    }
    run_command({"/usr/bin/borg", "list", "--short", borg_archive}, nullptr, &out_fd);

    auto out_contents = get_file_contents(out_fd, 0, -1);
    auto paths_present_in_backup = std::set<string_view>{};
    for (size_t beg = 0;;) {
        auto nl_pos = out_contents.find('\n', beg);
        if (nl_pos == string::npos) {
            break;
        }

        auto path = string_view{out_contents.data() + beg, nl_pos - beg};
        paths_present_in_backup.emplace(path);
        beg = nl_pos + 1;
    }

    string path;
    auto delete_absent_files =
        [&path, &paths_present_in_backup](auto& self, CStringView dir_path) -> void {
        for_each_dir_component(dir_path, [&](dirent* entry) {
            auto filename = string_view{entry->d_name};
            path += filename;
            if (path != DEFAULT_BACKUP_REPOSITORY_PATH) {
                bool absent = paths_present_in_backup.find(path) == paths_present_in_backup.end();
                if (entry->d_type == DT_DIR) {
                    path += '/';
                    self(self, path);
                    path.pop_back();
                    if (absent) {
                        stdlog("removing ", path);
                        if (rmdir(path.c_str())) {
                            THROW("rmdir()");
                        }
                    }
                } else if (absent) {
                    stdlog("removing ", path);
                    if (unlink(path.c_str())) {
                        THROW("unlink()");
                    }
                }
            }
            path.resize(path.size() - filename.size());
        });
    };
    delete_absent_files(delete_absent_files, ".");
    stdlog("... done.");

    stdlog("Restoring database...");
    {
        auto dump_sql_fd = FileDescriptor{"dump.sql", O_RDONLY};
        if (!dump_sql_fd.is_open()) {
            THROW("open()");
        }
        auto db_config = load_db_config();
        run_command(
            {
                "/usr/bin/mariadb",
                concat_tostr("--user=", db_config.user),
                concat_tostr("--password=", db_config.password),
                concat_tostr("--database=", db_config.db),
                concat_tostr("--host=", db_config.host),
            },
            &dump_sql_fd
        );
    }
    stdlog("... done.");

    stdlog("\033[1;32mRestoring sim is done.\033[m");
    stdlog(
        "\033[1;33mSim is not running. If needed, you have to start it manually with: ",
        get_cwd(),
        "manage start -b\033[m"
    );
}

} // namespace

int main(int argc, char** argv) {
    try {
        auto args = to_arg_seq(argc, argv);
        if (args.begin() == args.end()) {
            help(argv[0]);
            return 1;
        }

        auto it = args.begin();
        auto command = *it++;
        if (command == "list") {
            list(it, args.end());
        } else if (command == "save") {
            save(it, args.end());
        } else if (command == "restore") {
            restore(it, args.end());
        } else if (command == "help") {
            help(argv[0]);
        } else {
            help(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        die_with_error(e.what());
        return 1;
    }
}
