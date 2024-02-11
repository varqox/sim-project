#include "web_server/logs.hh"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/contest_files/contest_file.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/db/schema.hh>
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
#include <simlib/from_unsafe.hh>
#include <simlib/mysql/mysql.hh>
#include <simlib/opened_temporary_file.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/ranges.hh>
#include <simlib/sha.hh>
#include <simlib/simple_parser.hh>
#include <simlib/spawner.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_transform.hh>
#include <simlib/string_view.hh>
#include <simlib/temporary_file.hh>
#include <simlib/throw_assert.hh>
#include <simlib/time.hh>
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

// Update the below hash and body of the function do_perform_upgrade()
constexpr StringView NORMALIZED_SCHEMA_HASH_BEFORE_UPGRADE =
    "cf78d8088eec10a1ba2fe1e2d82bdb9697642e3d46458ef3d1dc3654bec355bd";

static void do_perform_upgrade(
    [[maybe_unused]] const string& sim_dir, [[maybe_unused]] mysql::Connection& mysql
) {
    // Upgrade here
    std::set<uint64_t, std::greater<>> contest_rounds_ids;

    std::map<uint64_t, std::string> contest_round_id_to_min_contest_problem_creation_time;
    auto res =
        mysql.query("SELECT contest_round_id, MIN(created_at) FROM contest_problems GROUP BY contest_round_id");
    while (res.next()) {
        auto contest_round_id = str2num<uint64_t>(res[0]).value();
        contest_rounds_ids.emplace(contest_round_id);
        contest_round_id_to_min_contest_problem_creation_time.emplace(contest_round_id, res[1].to_string());
    }

    // mysql.update("UNLOCK TABLES"); // Seems like a bug in MariaDB that we need it before the next query
    std::map<uint64_t, std::string> contest_round_id_to_min_job_creation_time;
    res = mysql.query("SELECT aux_id, MIN(created_at) FROM jobs WHERE type=11 GROUP BY aux_id");
    while (res.next()) {
        auto contest_round_id = str2num<uint64_t>(res[0]).value();
        contest_rounds_ids.emplace(contest_round_id);
        contest_round_id_to_min_job_creation_time.emplace(contest_round_id, res[1].to_string());
    }

    res = mysql.query("SELECT id from contest_rounds");
    while (res.next()) {
        auto contest_round_id = str2num<uint64_t>(res[0]).value();
        contest_rounds_ids.emplace(contest_round_id);
    }

    mysql.update(
        "ALTER TABLE contest_rounds ADD COLUMN created_at datetime NULL DEFAULT NULL AFTER id"
    );

    auto min_date = mysql_date();
    for (auto contest_round_id : contest_rounds_ids) {
        auto it = contest_round_id_to_min_contest_problem_creation_time.find(contest_round_id);
        if (it != contest_round_id_to_min_contest_problem_creation_time.end()) {
            min_date = std::min(min_date, it->second);
        }
        it = contest_round_id_to_min_job_creation_time.find(contest_round_id);
        if (it != contest_round_id_to_min_job_creation_time.end()) {
            min_date = std::min(min_date, it->second);
        }

        stdlog(contest_round_id, ' ', min_date);
        mysql.prepare("UPDATE contest_rounds SET created_at=? WHERE id=?")
            .bind_and_execute(min_date, contest_round_id);
    }

    mysql.update("ALTER TABLE contest_rounds MODIFY COLUMN created_at datetime NOT NULL");
}

enum class LockKind {
    READ,
    WRITE,
};
static void lock_all_tables(mysql::Connection& mysql, LockKind lock_kind);

static int perform_upgrade(const string& sim_dir, mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;
    stdlog("\033[1;34mChecking if upgrade is needed.\033[m");
    lock_all_tables(mysql, LockKind::READ);
    auto normalized_schema = normalized(sim::db::get_db_schema(mysql));
    auto normalized_schema_hash = sha3_256(normalized_schema);

    auto normalized_schema_after_upgrade = normalized(sim::db::schema);
    auto normalized_schema_hash_after_upgrade = sha3_256(normalized_schema_after_upgrade);

    if (normalized_schema_hash == normalized_schema_hash_after_upgrade) {
        stdlog("\033[1;32mNo upgrade is needed.\033[m");
        return 0;
    }
    if (normalized_schema_hash == NORMALIZED_SCHEMA_HASH_BEFORE_UPGRADE) {
        stdlog("\033[1;34mStarted the sim upgrade.\033[m");
        lock_all_tables(mysql, LockKind::WRITE);
        do_perform_upgrade(sim_dir, mysql);
        stdlog("\033[1;32mSim upgrading is complete.\033[m");

        normalized_schema = normalized(sim::db::get_db_schema(mysql));
        normalized_schema_hash = sha3_256(normalized_schema);
        stdlog("hash of the normalized schema after upgrade = ", normalized_schema_hash);
        if (normalized_schema_hash != normalized_schema_hash_after_upgrade) {
            stdlog("\033[1;31mUpgrade succeeded but the hash of the normalized schema is different "
                   "than expected\033[m");
            return 1;
        }
        return 0;
    }
    if (normalized_schema.empty()) {
        stdlog("\033[1;34mDatabase is empty. Started initializing the database.\033[m");
        for (const auto& table_schema : sim::db::schema.table_schemas) {
            mysql.update(table_schema.create_table_sql);
        }
        // Create sim root user
        {
            auto [salt, hash] = sim::users::salt_and_hash_password("sim");
            decltype(sim::users::User::type) type = sim::users::User::Type::ADMIN;
            mysql
                .prepare(
                    "INSERT INTO users (id, created_at, type, username, first_name, last_name, "
                    "email, password_salt, password_hash) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
                )
                .bind_and_execute(
                    sim::users::SIM_ROOT_UID,
                    mysql_date(),
                    type,
                    "sim",
                    "sim",
                    "sim",
                    "sim@sim",
                    salt,
                    hash
                );
        }
        stdlog("\033[1;32mDatabase initialized.\033[m");
        return 0;
    }

    stdlog("\033[1;31mCannot upgrade, unknown schema hash = ", normalized_schema_hash, "\033[m");
    // Dump current normalized schema
    assert(has_prefix(sim_dir, "/"));
    auto dump_path = path_absolute(from_unsafe{concat_tostr(sim_dir, "normalized_schema.sql")});
    put_file_contents(dump_path, from_unsafe{concat_tostr(normalized_schema, '\n')});
    stdlog("Dumped the current normalized schema to file: ", dump_path);
    // Dump expected normalized schema
    dump_path = path_absolute(from_unsafe{concat_tostr(sim_dir, "expected_normalized_schema.sql")});
    put_file_contents(dump_path, from_unsafe{concat_tostr(normalized_schema_after_upgrade, '\n')});
    stdlog("Dumped the expected normalized schema to file: ", dump_path);
    return 1;
}

static void lock_all_tables(mysql::Connection& mysql, LockKind lock_kind) {
    std::string query = "LOCK TABLES";
    bool first = true;
    for (const auto& table_name : sim::db::get_all_table_names(mysql)) {
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
    if (!first) {
        // There are tables to lock
        mysql.update(query);
    }
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

    sim::mysql::Connection mysql;
    try {
        // Get database connection
        mysql = sim::mysql::make_conn_with_credential_file(concat(sim_dir, ".db.config"));
    } catch (const std::exception& e) {
        errlog("\033[31mFailed to connect to database\033[m - ", e.what());
        return 1;
    }

    return perform_upgrade(sim_dir, mysql);
}

int main(int argc, char** argv) {
    try {
        return true_main(argc, argv);
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        throw;
    }
}
