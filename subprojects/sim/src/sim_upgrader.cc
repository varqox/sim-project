#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <memory>
#include <regex>
#include <sim/change_problem_statement_jobs/change_problem_statement_job.hh>
#include <sim/db/schema.hh>
#include <sim/jobs/job.hh>
#include <sim/jobs/utils.hh>
#include <sim/mysql/mysql.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <sim/users/user.hh>
#include <simlib/concat.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/file_contents.hh>
#include <simlib/from_unsafe.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/path.hh>
#include <simlib/process.hh>
#include <simlib/sha.hh>
#include <simlib/spawner.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <unistd.h>

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
    "455518296186977ca9e170ee797a83f864000d71a3718b7a87c36c70810eceed";

static void do_perform_upgrade(
    [[maybe_unused]] const string& sim_dir, [[maybe_unused]] sim::mysql::Connection& mysql
) {
    STACK_UNWINDING_MARK;

    // Upgrade here
    auto sim_conf_contents = get_file_contents(concat_tostr(sim_dir, "sim.conf"));
    {
        ConfigFile sim_conf;
        sim_conf.add_vars("address");
        sim_conf.load_config_from_string(sim_conf_contents);
        const auto& var = sim_conf.get_var("address");
        auto val_span = var.value_span();
        auto pos = sim_conf_contents.rfind("\naddress:", val_span.beg - 1);
        throw_assert(pos != std::string::npos);
        sim_conf_contents = concat_tostr(
            substring(sim_conf_contents, 0, pos),
            "\nweb_server_address: ",
            substring(sim_conf_contents, val_span.beg)
        );
    }
    {
        ConfigFile sim_conf;
        sim_conf.add_vars("workers");
        sim_conf.load_config_from_string(sim_conf_contents);
        const auto& var = sim_conf.get_var("workers");
        auto val_span = var.value_span();
        auto pos = sim_conf_contents.rfind("\nworkers:", val_span.beg - 1);
        throw_assert(pos != std::string::npos);
        sim_conf_contents = concat_tostr(
            substring(sim_conf_contents, 0, pos),
            "\nweb_server_workers: ",
            substring(sim_conf_contents, val_span.beg)
        );
    }
    {
        ConfigFile sim_conf;
        sim_conf.add_vars("connections");
        sim_conf.load_config_from_string(sim_conf_contents);
        const auto& var = sim_conf.get_var("connections");
        auto val_span = var.value_span();
        auto pos = sim_conf_contents.rfind("\nconnections:", val_span.beg - 1);
        throw_assert(pos != std::string::npos);
        sim_conf_contents = concat_tostr(
            substring(sim_conf_contents, 0, pos),
            "\nweb_server_connections: ",
            substring(sim_conf_contents, val_span.beg)
        );
    }
    {
        sim_conf_contents = std::regex_replace(
            sim_conf_contents,
            std::regex{"\n\n# Number of job server's judge workers \\(cannot be lower than 1\\)"},
            ""
        );
        sim_conf_contents = std::regex_replace(
            sim_conf_contents,
            std::regex{"\n# Number of job server's local workers \\(cannot be lower than 1\\)"},
            "\n# Number of job server workers (cannot be lower than 1)"
        );
    }
    size_t workers = 0;
    {
        ConfigFile sim_conf;
        sim_conf.add_vars("js_judge_workers");
        sim_conf.load_config_from_string(sim_conf_contents);
        const auto& var = sim_conf.get_var("js_judge_workers");
        workers = var.as<size_t>().value_or(0);
        if (workers < 1) {
            THROW("sim.conf: Invalid number of js_judge_workers");
        }
        auto val_span = var.value_span();
        auto pos = sim_conf_contents.rfind("\njs_judge_workers:", val_span.beg - 1);
        throw_assert(pos != std::string::npos);
        sim_conf_contents = concat_tostr(
            substring(sim_conf_contents, 0, pos), substring(sim_conf_contents, val_span.end)
        );
    }
    {
        ConfigFile sim_conf;
        sim_conf.add_vars("js_local_workers");
        sim_conf.load_config_from_string(sim_conf_contents);
        const auto& var = sim_conf.get_var("js_local_workers");
        auto local_workers = var.as<size_t>().value_or(0);
        if (local_workers < 1) {
            THROW("sim.conf: Invalid number of js_local_workers");
        }
        workers += local_workers;
        auto val_span = var.value_span();
        auto pos = sim_conf_contents.rfind("\njs_local_workers:", val_span.beg - 1);
        throw_assert(pos != std::string::npos);
        sim_conf_contents = concat_tostr(
            substring(sim_conf_contents, 0, pos),
            "\njob_server_workers: ",
            workers,
            substring(sim_conf_contents, val_span.end)
        );
        stdlog(sim_conf_contents);
    }
    put_file_contents(concat_tostr(sim_dir, "sim.conf"), sim_conf_contents);

    mysql.execute("UNLOCK TABLES");
    mysql.execute("UPDATE jobs SET status=1 WHERE status=2");
    mysql.execute("ALTER TABLE submissions RENAME COLUMN last_judgment TO last_judgment_began_at");
    mysql.execute("ALTER TABLE submissions RENAME COLUMN contest_final TO contest_problem_final");
    mysql.execute("ALTER TABLE submissions RENAME COLUMN contest_initial_final TO "
                  "contest_problem_initial_final");
    mysql.execute(
        "ALTER TABLE submissions MODIFY COLUMN last_judgment_began_at datetime DEFAULT NULL"
    );

    mysql.execute("ALTER TABLE submissions ADD COLUMN initial_final_candidate tinyint(1) NOT NULL "
                  "DEFAULT 0 AFTER language");
    mysql.execute(sim::sql::Update("submissions")
                      .set("initial_final_candidate=1")
                      .where(
                          "initial_status IN (?, ?, ?, ?, ?, ?) AND type=?",
                          sim::submissions::Submission::Status::OK,
                          sim::submissions::Submission::Status::WA,
                          sim::submissions::Submission::Status::TLE,
                          sim::submissions::Submission::Status::MLE,
                          sim::submissions::Submission::Status::OLE,
                          sim::submissions::Submission::Status::RTE,
                          sim::submissions::Submission::Type::NORMAL
                      ));

    mysql.execute("DROP INDEX final1 ON submissions");
    mysql.execute("DROP INDEX final2 ON submissions");
    mysql.execute("DROP INDEX final3 ON submissions");
    mysql.execute("DROP INDEX initial_final2 ON submissions");
    mysql.execute("DROP INDEX initial_final3 ON submissions");
    // Needed to efficiently select problem final submission
    mysql.execute("CREATE INDEX `for_problem_final` ON submissions (`final_candidate`, `user_id`, "
                  "`problem_id`, `score` DESC, `full_status`, `id` DESC)");
    // Needed to efficiently select contest problem final submission
    mysql.execute("CREATE INDEX `for_contest_problem_final_latest` ON submissions "
                  "(`final_candidate`, `user_id`, `contest_problem_id`, `id` DESC)");
    mysql.execute(
        "CREATE INDEX `for_contest_problem_final_by_score` ON submissions (`final_candidate`, "
        "`user_id`, `contest_problem_id`, `score` DESC, `full_status`, `id` DESC)"
    );
    // Needed to efficiently select contest problem initial final submission
    mysql.execute("CREATE INDEX `for_contest_initial_problem_final_latest` ON submissions "
                  "(`initial_final_candidate`, `user_id`, `contest_problem_id`, `id` DESC)");
    mysql.execute(
        "CREATE INDEX `for_contest_initial_problem_final_by_initial_status` ON submissions "
        "(`initial_final_candidate`, `user_id`, `contest_problem_id`, `initial_status`, `id` DESC)"
    );
    mysql.execute("CREATE INDEX `for_contest_initial_problem_final_by_score_and_initial_status` ON "
                  "submissions (`initial_final_candidate`, `user_id`, `contest_problem_id`, "
                  "`score` DESC, `initial_status`, `id` DESC)");
    mysql.execute("CREATE INDEX `for_contest_initial_problem_final_by_score_and_full_status` ON "
                  "submissions (`initial_final_candidate`, `user_id`, `contest_problem_id`, "
                  "`score` DESC, `full_status`, `id` DESC)");

    auto stmt = mysql.execute(sim::sql::Select("user_id, problem_id, contest_problem_id")
                                  .from("submissions")
                                  .group_by("user_id, problem_id, contest_problem_id"));
    using sim::submissions::Submission;
    decltype(Submission::user_id) submission_user_id;
    decltype(Submission::problem_id) submission_problem_id;
    decltype(Submission::contest_problem_id) submission_contest_problem_id;
    stmt.res_bind(submission_user_id, submission_problem_id, submission_contest_problem_id);
    while (stmt.next()) {
        stdlog(
            "update_final(",
            submission_user_id.value_or(0),
            ", ",
            submission_problem_id,
            ", ",
            submission_contest_problem_id.value_or(0),
            ")"
        );
        sim::submissions::update_final(
            mysql, submission_user_id, submission_problem_id, submission_contest_problem_id
        );
    }
}

enum class LockKind {
    READ,
    WRITE,
};
static void lock_all_tables(sim::mysql::Connection& mysql, LockKind lock_kind);

static int perform_upgrade(const string& sim_dir, sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;
    stdlog("\033[1;34mChecking if upgrade is needed.\033[m");
    lock_all_tables(mysql, LockKind::READ);
    auto normalized_schema = normalized(sim::db::get_db_schema(mysql));
    auto normalized_schema_hash = sha3_256(normalized_schema);

    auto normalized_schema_after_upgrade = normalized(sim::db::get_schema());
    auto normalized_schema_hash_after_upgrade = sha3_256(normalized_schema_after_upgrade);

    if (normalized_schema == normalized_schema_after_upgrade) {
        stdlog("\033[1;32mNo upgrade is needed.\033[m");
        return 0;
    }
    if (normalized_schema_hash == NORMALIZED_SCHEMA_HASH_BEFORE_UPGRADE) {
        stdlog("\033[1;34mStarted the sim upgrade.\033[m");
        lock_all_tables(mysql, LockKind::WRITE);
        do_perform_upgrade(sim_dir, mysql);
        stdlog("\033[1;32mSim upgrading is complete.\033[m");

        normalized_schema = normalized(sim::db::get_db_schema(mysql));
        stdlog("hash of the normalized schema after upgrade = ", sha3_256(normalized_schema));
        if (normalized_schema != normalized_schema_after_upgrade) {
            stdlog("\033[1;31mUpgrade succeeded but the normalized schema is different than "
                   "expected\033[m");
            return 1;
        }
        return 0;
    }
    if (normalized_schema.empty()) {
        stdlog("\033[1;34mDatabase is empty. Started initializing the database.\033[m");
        for (const auto& table_schema : sim::db::get_schema().table_schemas) {
            mysql.update(table_schema.create_table_sql);
        }
        // Create sim root user
        {
            auto [salt, hash] = sim::users::salt_and_hash_password("sim");
            mysql.execute(sim::sql::InsertInto("users (id, created_at, type, username, first_name, "
                                               "last_name, email, password_salt, password_hash)")
                              .values(
                                  "?, ?, ?, ?, ?, ?, ?, ?, ?",
                                  sim::users::SIM_ROOT_ID,
                                  utc_mysql_datetime(),
                                  sim::users::User::Type::ADMIN,
                                  "sim",
                                  "sim",
                                  "sim",
                                  "sim@sim",
                                  salt,
                                  hash
                              ));
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

static void lock_all_tables(sim::mysql::Connection& mysql, LockKind lock_kind) {
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

    std::unique_ptr<sim::mysql::Connection> mysql;
    try {
        // Get database connection
        // NOLINTNEXTLINE(modernize-make-unique)
        mysql = std::unique_ptr<sim::mysql::Connection>{new sim::mysql::Connection(
            sim::mysql::Connection::from_credential_file(concat_tostr(sim_dir, ".db.config").c_str()
            )
        )};
    } catch (const std::exception& e) {
        errlog("\033[31mFailed to connect to database\033[m - ", e.what());
        return 1;
    }

    return perform_upgrade(sim_dir, *mysql);
}

int main(int argc, char** argv) {
    try {
        return true_main(argc, argv);
    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        throw;
    }
}
