#include "simlib/mysql/mysql.hh"
#include "simlib/path.hh"
#include "simlib/sha.hh"
#include "simlib/simple_parser.hh"
#include "simlib/string_traits.hh"
#include "simlib/throw_assert.hh"
#include "sql_tables.hh"
#include "web_server/logs.hh"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <regex>
#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/contest_files/contest_file.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/inf_datetime.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problem_tags/problem_tag.hh>
#include <sim/users/user.hh>
#include <simlib/concat.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/defer.hh>
#include <simlib/err_defer.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_info.hh>
#include <simlib/file_manip.hh>
#include <simlib/opened_temporary_file.hh>
#include <simlib/process.hh>
#include <simlib/ranges.hh>
#include <simlib/spawner.hh>
#include <simlib/string_view.hh>
#include <simlib/temporary_file.hh>
#include <unistd.h>
#include <utility>

using std::string;
using std::vector;

[[maybe_unused]] static void
run_command(const vector<string>& args, const Spawner::Options& options = {}) {
    auto es = Spawner::run(args[0], args, options);
    if (es.si.code != CLD_EXITED or es.si.status != 0) {
        THROW(args[0], " failed: ", es.message);
    }
}

// Update the below hashes and body of the function do_perform_upgrade()
constexpr StringView SCHEMA_HASH_BEFORE_UPGRADE =
    "2ea89e5d6caf5e6a5792a064536762ff5e4bbc59e02b9fc437f92e16abec173d";
constexpr StringView SCHEMA_HASH_AFTER_UPGRADE =
    "2ea89e5d6caf5e6a5792a064536762ff5e4bbc59e02b9fc437f92e16abec173d";

static void do_perform_upgrade(
    [[maybe_unused]] const string& sim_dir, [[maybe_unused]] mysql::Connection& conn
) {
    // Upgrade here
}

enum class LockKind {
    READ,
    WRITE,
};
static void lock_all_tables(mysql::Connection& conn, LockKind lock_kind);
static string get_db_schema(mysql::Connection& conn);

static int perform_upgrade(const string& sim_dir, mysql::Connection& conn) {
    STACK_UNWINDING_MARK;
    stdlog("\033[1;34mChecking if upgrade is needed.\033[m");
    lock_all_tables(conn, LockKind::READ);
    auto schema = get_db_schema(conn);
    auto schema_hash = sha3_256(schema);
    if (schema_hash == SCHEMA_HASH_AFTER_UPGRADE) {
        stdlog("\033[1;32mNo upgrade is needed.\033[m");
        return 0;
    }
    if (schema_hash == SCHEMA_HASH_BEFORE_UPGRADE) {
        stdlog("\033[1;34mStarted the sim upgrade.\033[m");
        lock_all_tables(conn, LockKind::WRITE);
        do_perform_upgrade(sim_dir, conn);
        stdlog("\033[1;32mSim upgrading is complete.\033[m");

        auto schema_hash_after_upgrade = sha3_256(from_unsafe{get_db_schema(conn)});
        stdlog("schema hash after upgrade = ", schema_hash_after_upgrade);
        if (schema_hash_after_upgrade != SCHEMA_HASH_AFTER_UPGRADE) {
            stdlog(
                "\033[1;31mUpgrade succeeded but the schema hash is different than expected\033[m"
            );
            return 1;
        }
        return 0;
    }

    stdlog("\033[1;31mCannot upgrade, unknown schema hash = ", schema_hash, "\033[m");
    auto schema_dump_path = concat_tostr(sim_dir, "schema_dump.sql");
    put_file_contents(schema_dump_path, schema);
    stdlog("Dumped the current schema to file: ", schema_dump_path);
    return 1;
}

static vector<string> get_all_table_names(mysql::Connection& conn) {
    vector<string> table_names;
    auto res = conn.query("SHOW TABLES");
    while (res.next()) {
        table_names.emplace_back(res[0].to_string());
    }
    sort(table_names.begin(), table_names.end());
    return table_names;
}

static void lock_all_tables(mysql::Connection& conn, LockKind lock_kind) {
    std::string query = "LOCK TABLES";
    bool first = true;
    for (const auto& table_name : get_all_table_names(conn)) {
        if (first) {
            first = false;
        } else {
            query += ',';
        }
        back_insert(query, " `", table_name, "` ");
        switch (lock_kind) {
        case LockKind::READ: query += "READ"; break;
        case LockKind::WRITE: query += "WRITE"; break;
        }
    }
    conn.update(query);
}

static string get_db_schema(mysql::Connection& conn) {
    string schema;
    for (const auto& table_name : get_all_table_names(conn)) {
        auto res = conn.query("SHOW CREATE TABLE `", table_name, '`');
        throw_assert(res.next());
        auto table_schema = res[1].to_string();
        // Remove AUTOINCREMENT=
        table_schema =
            std::regex_replace(table_schema, std::regex{R"((\n\).*) AUTO_INCREMENT=\w+)"}, "$1");
        // Split schema to rows
        vector<StringView> rows;
        {
            auto table_schema_parser = SimpleParser{table_schema};
            auto row = table_schema_parser.extract_next_non_empty('\n');
            while (!row.empty()) {
                rows.emplace_back(row);
                row = table_schema_parser.extract_next_non_empty('\n');
            }
        }
        // Sort table schema by rows
        auto row_to_kind = [](StringView row) {
            if (has_prefix(row, "CREATE TABLE ")) {
                return 0;
            }
            if (has_prefix(row, "  `")) {
                return 1;
            }
            if (has_prefix(row, "  PRIMARY KEY ")) {
                return 2;
            }
            if (has_prefix(row, "  UNIQUE KEY ")) {
                return 3;
            }
            if (has_prefix(row, "  KEY ")) {
                return 4;
            }
            if (has_prefix(row, "  CONSTRAINT ")) {
                return 5;
            }
            if (has_prefix(row, ")")) {
                return 6;
            }
            THROW("BUG: unknown prefix");
        };
        std::sort(rows.begin(), rows.end(), [&](StringView a, StringView b) {
            return std::pair{row_to_kind(a), a} < std::pair{row_to_kind(b), b};
        });
        // Append the schema
        for (StringView row : rows) {
            // Remove comma before end of table definition
            if (has_prefix(row, ")") && has_suffix(schema, ",\n")) {
                schema.pop_back();
                schema.back() = '\n';
            }
            back_insert(schema, row);
            // Append missing commas
            if (has_prefix(row, "  ") && !has_suffix(row, ",")) {
                schema += ',';
            }
            // Append semicolon after table definition
            if (has_prefix(row, ")")) {
                schema += ';';
            }
            schema += '\n';
        }
    }
    return schema;
}

static void print_help(const char* program_name) {
    if (not program_name) {
        program_name = "sim-upgrader";
    }

    errlog.label(false);
    errlog(
        "Usage: ",
        program_name,
        " [options]\n"
        "\n"
        "Options:\n"
        "  -h, --help            Display this information\n"
        "  -q, --quiet           Quiet mode"
    );
}

namespace {

struct CmdOptions {};

} // namespace

static CmdOptions parse_cmd_options(int& argc, char** argv) {
    STACK_UNWINDING_MARK;

    int new_argc = 1;
    CmdOptions cmd_options;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (0 == strcmp(argv[i], "-h") or 0 == strcmp(argv[i], "--help")) {
                print_help(argv[0]); // argv[0] is valid (argc > 1)
                _exit(0);
            } else if (0 == strcmp(argv[i], "-q") or 0 == strcmp(argv[i], "--quiet")) {
                stdlog.open("/dev/null");
            } else {
                (void)fprintf(stderr, "Unknown option: '%s'\n", argv[i]);
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

    // Signal control: ignore all common signals. The upgrade process cannot be interrupted.
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN; // NOLINT

    (void)sigaction(SIGINT, &sa, nullptr);
    (void)sigaction(SIGTERM, &sa, nullptr);
    (void)sigaction(SIGQUIT, &sa, nullptr);

    stdlog.use(stdout);
    errlog.use(stderr);

    parse_cmd_options(argc, argv);
    if (argc != 1) {
        print_help(argv[0]);
        return 1;
    }

    auto sim_dir = executable_path(getpid());
    sim_dir.resize(path_dirpath(sim_dir).size());
    sim_dir += "../";

    sim::mysql::Connection conn;
    try {
        // Get database connection
        conn = sim::mysql::make_conn_with_credential_file(concat(sim_dir, ".db.config"));
    } catch (const std::exception& e) {
        errlog("\033[31mFailed to connect to database\033[m - ", e.what());
        return 1;
    }

    return perform_upgrade(sim_dir, conn);
}

int main(int argc, char** argv) {
    try {
        return true_main(argc, argv);
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        throw;
    }
}
