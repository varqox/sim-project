#include "sim/contest_entry_tokens/contest_entry_token.hh"
#include "sim/contest_files/contest_file.hh"
#include "sim/contest_rounds/contest_round.hh"
#include "sim/contest_users/contest_user.hh"
#include "sim/inf_datetime.hh"
#include "sim/mysql/mysql.hh"
#include "sim/problem_tags/problem_tag.hh"
#include "sim/users/user.hh"
#include "simlib/concat.hh"
#include "simlib/concat_tostr.hh"
#include "simlib/config_file.hh"
#include "simlib/defer.hh"
#include "simlib/err_defer.hh"
#include "simlib/file_contents.hh"
#include "simlib/file_descriptor.hh"
#include "simlib/file_info.hh"
#include "simlib/file_manip.hh"
#include "simlib/opened_temporary_file.hh"
#include "simlib/process.hh"
#include "simlib/ranges.hh"
#include "simlib/spawner.hh"
#include "simlib/string_view.hh"
#include "simlib/temporary_file.hh"
#include "src/sql_tables.hh"
#include "src/web_server/logs.hh"

#include <climits>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
mysql::Connection conn;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
InplaceBuff<PATH_MAX> sim_build; // with trailing '/'

struct CmdOptions {
    bool reset_new_problems_time_limits = false;
};

} // namespace

[[maybe_unused]] static void run_command(
        const std::vector<std::string>& args, const Spawner::Options& options = {}) {
    auto es = Spawner::run(args[0], args, options);
    if (es.si.code != CLD_EXITED or es.si.status != 0) {
        THROW(args[0], " failed: ", es.message);
    }
}

// Works if and only if:
// - no table is renamed
// - no column is added / removed
// - columns stay in the same order
// - column's type is convertible to column's new type compatible
template <class Func>
static void update_db_schema(Func&& prepare_database) {
    OpenedTemporaryFile mysql_cnf("/tmp/sim-upgrade-mysql-cnf.XXXXXX");
    write_all_throw(mysql_cnf,
            intentional_unsafe_string_view(concat("[client]\nuser=\"", conn.impl()->user,
                    "\"\npassword=\"", conn.impl()->passwd, "\"\n")));

    static constexpr auto backup_table_suffix = "_bkp";
    auto drop_backup_tables = [&] {
        for (auto table : reverse_view(tables)) {
            conn.update("DROP TABLE IF EXISTS `", table, backup_table_suffix, '`');
        }
    };

    TemporaryFile db_backup("/tmp/sim-upgrade-db-backup.XXXXXX");
    TemporaryFile db_dump("/tmp/sim-upgrade-db-dump.XXXXXX");

    drop_backup_tables();
    run_command({
            "mysqldump",
            concat_tostr("--defaults-file=", mysql_cnf.path()),
            concat_tostr("--result-file=", db_backup.path()),
            "--single-transaction",
            conn.impl()->db,
    });

    ErrDefer save_backup = [&] {
        for (int i = 0; i < 100000; ++i) {
            auto filename = concat("db-backup.", i);
            if (access(filename, F_OK) == 0) {
                continue;
            }
            if (copy(db_backup.path(), filename) == 0) {
                stdlog("Database backup saved as: ", filename);
                break;
            }
        }
    };
    {
        ErrDefer restore_from_backup = [&] {
            stdlog("Restore tables from dump-backup");
            run_command(
                    {
                            "mysql",
                            concat_tostr("--defaults-file=", mysql_cnf.path()),
                            conn.impl()->db,
                    },
                    Spawner::Options{FileDescriptor{db_backup.path(), O_RDONLY}, STDOUT_FILENO,
                            STDERR_FILENO, "."});
            drop_backup_tables();
        };
        prepare_database();
        // Save records for reinsertion in the new schema
        run_command({
                "mysqldump",
                concat_tostr("--defaults-file=", mysql_cnf.path()),
                concat_tostr("--result-file=", db_dump.path()),
                "--single-transaction",
                "-t",
                conn.impl()->db,
        });
        // Rename tables (as a fast backup)
        stdlog("Rename tables (as a backup)");
        std::string query = "RENAME TABLE ";
        for (auto table : tables) {
            if (table != tables.front()) {
                back_insert(query, ", ");
            }
            back_insert(query, '`', table, "` TO `", table, backup_table_suffix, '`');
        }
        conn.update(query);
    }
    bool done = false;
    ErrDefer restore_by_rename = [&] {
        if (done) {
            return;
        }
        stdlog("Restore backup via renaming tables");
        // First drop tables that we will rename to
        for (auto table : reverse_view(tables)) {
            conn.update("DROP TABLE IF EXISTS `", table, '`');
        }
        std::string query = "RENAME TABLE ";
        for (auto table : tables) {
            if (table != tables.front()) {
                back_insert(query, ", ");
            }
            back_insert(query, '`', table, backup_table_suffix, "` TO `", table, '`');
        }
        conn.update(query);
    };

    run_command({"build/setup-installation", "--drop-tables", sim_build.to_string()});
    conn.update("DELETE FROM users"); // Remove the just created root account
    run_command(
            {
                    "mysql",
                    concat_tostr("--defaults-file=", mysql_cnf.path()),
                    conn.impl()->db,
            },
            Spawner::Options{
                    FileDescriptor{db_dump.path(), O_RDONLY}, STDOUT_FILENO, STDERR_FILENO, "."});
    done = true;
    drop_backup_tables();
}

static int perform_upgrade() {
    STACK_UNWINDING_MARK;

    conn.update("ALTER TABLE problems RENAME COLUMN owner TO owner_id");
    conn.update("ALTER TABLE problems RENAME COLUMN added TO created_at");
    conn.update("ALTER TABLE problems RENAME COLUMN last_edit TO updated_at");

    // update_db_schema([&] { conn.update("RENAME TABLE session TO sessions"); });

    stdlog("\033[1;32mSim upgrading is complete\033[m");
    return 0;
}

static void print_help(const char* program_name) {
    if (not program_name) {
        program_name = "sim-upgrader";
    }

    errlog.label(false);
    errlog("Usage: ", program_name,
            " [options] <sim_build>\n"
            "  Where sim_build is a path to build directory of a Sim to upgrade\n"
            "\n"
            "Options:\n"
            "  -h, --help            Display this information\n"
            "  -q, --quiet           Quiet mode");
}

static CmdOptions parse_cmd_options(int& argc, char** argv) {
    STACK_UNWINDING_MARK;

    int new_argc = 1;
    CmdOptions cmd_options;
    for (int i = 1; i < argc; ++i) {

        if (argv[i][0] == '-') {
            if (0 == strcmp(argv[i], "-h") or 0 == strcmp(argv[i], "--help")) { // Help
                print_help(argv[0]); // argv[0] is valid (argc > 1)
                exit(0);

            } else if (0 == strcmp(argv[i], "-q") or 0 == strcmp(argv[i], "--quiet"))
            { // Quiet mode
                stdlog.open("/dev/null");

            } else { // Unknown
                eprintf("Unknown option: '%s'\n", argv[i]);
            }

        } else {
            argv[new_argc++] = argv[i];
        }
    }

    argc = new_argc;
    argv[argc] = nullptr;
    return cmd_options;
}

static int true_main(int argc, char** argv) {
    STACK_UNWINDING_MARK;

    // Signal control
    struct sigaction sa {};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN; // NOLINT

    (void)sigaction(SIGINT, &sa, nullptr);
    (void)sigaction(SIGTERM, &sa, nullptr);
    (void)sigaction(SIGQUIT, &sa, nullptr);

    stdlog.use(stdout);
    errlog.use(stderr);

    CmdOptions cmd_options = parse_cmd_options(argc, argv);
    (void)cmd_options;
    if (argc != 2) {
        print_help(argv[0]);
        return 1;
    }

    sim_build.append(argv[1]);
    if (not has_suffix(sim_build, "/")) {
        sim_build.append('/');
    }

    try {
        // Get connection
        conn = sim::mysql::make_conn_with_credential_file(concat(sim_build, ".db.config"));
    } catch (const std::exception& e) {
        errlog("\033[31mFailed to connect to database\033[m - ", e.what());
        return 1;
    }

    auto manage_path = concat_tostr(sim_build, "manage");
    // Stop web server and job server
    Spawner::run(manage_path, {manage_path, "stop"});

    Defer servers_restorer([&] {
        try {
            stdlog("Restoring servers");
        } catch (...) {
        }

        Spawner::run(manage_path, {manage_path, "-b", "start"});
    });

    return perform_upgrade();
}

int main(int argc, char** argv) {
    try {
        return true_main(argc, argv);
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        throw;
    }
}
