#include "sql_tables.hh"
#include "web_server/users/api.hh"
#include <fcntl.h>
#include <iostream>
#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/contest_files/contest_file.hh>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/contests/contest.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problem_tags/problem_tag.hh>
#include <sim/sessions/session.hh>
#include <sim/users/user.hh>
#include <simlib/concat.hh>
#include <simlib/concat_tostr.hh>
#include <simlib/config_file.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/file_info.hh>
#include <simlib/file_path.hh>
#include <simlib/file_perms.hh>
#include <simlib/inplace_buff.hh>
#include <simlib/random.hh>
#include <simlib/ranges.hh>
#include <simlib/sha.hh>
#include <simlib/string_view.hh>

using std::array;
using std::string;

/**
 * @brief Displays help
 */
static void help(const char* program_name) {
    if (program_name == nullptr) {
        program_name = "setup-installation";
    }

    printf("Usage: %s [options] INSTALL_DIR\n", program_name);
    puts("Setup database after SIM installation");
    puts("");
    puts("Options:");
    puts("  --drop-tables          Drop database tables before recreating "
         "them");
    puts("  -h, --help             Display this information");
    puts("  --only-drop-tables     Drop database tables and exit");
}

struct CmdOptions {
    bool drop_tables = false;
    bool only_drop_tables = false;
};

static CmdOptions parse_cmd_options(int& argc, char** argv) {
    CmdOptions cmd_options;
    int new_argc = 1;
    for (int i = 1; i < argc; ++i) {

        if (argv[i][0] == '-') {
            // Drop tables
            if (0 == strcmp(argv[i], "--drop-tables")) {
                cmd_options.drop_tables = true;
            }
            // Help
            else if (0 == strcmp(argv[i], "-h") || 0 == strcmp(argv[i], "--help"))
            {
                help(argv[0]); // argv[0] is valid (argc > 1)
                exit(0);
            }
            // Drop tables
            else if (0 == strcmp(argv[i], "--only-drop-tables"))
            {
                cmd_options.drop_tables = true;
                cmd_options.only_drop_tables = true;
            }
            // Unknown
            else
            {
                errlog("Unknown option: '", argv[i], '\'');
            }
        } else {
            argv[new_argc++] = argv[i];
        }
    }

    argc = new_argc;
    return cmd_options;
}

static void create_db_config(FilePath db_config_path) {
    string user;
    string password;
    string database;
    string host;
    stdlog("Type MariaDB user to use:");
    getline(std::cin, user);
    stdlog("Type password for the MariaDB user:");
    getline(std::cin, password);
    stdlog("Type name of the MariaDB database for Sim to use:");
    getline(std::cin, database);
    stdlog("Type name or IP address of the MariaDB database [localhost]:");
    getline(std::cin, host);
    if (host.empty()) {
        host = "localhost";
    }

    FileDescriptor fd(db_config_path, O_CREAT | O_TRUNC | O_WRONLY, S_0600);
    write_all_throw(fd,
            intentional_unsafe_string_view(concat("user: ", ConfigFile::escape_string(user),
                    "\npassword: ", ConfigFile::escape_string(password),
                    "\ndb: ", ConfigFile::escape_string(database),
                    "\nhost: ", ConfigFile::escape_string(host), '\n')));
}

struct TryToCreateTable {
    bool error = false;
    mysql::Connection& conn_;
    std::array<CStringView, 13> sorted_tables = tables;

    explicit TryToCreateTable(mysql::Connection& conn)
    : conn_(conn) {
        std::sort(sorted_tables.begin(), sorted_tables.end());
    }

    template <class Str, class Func>
    void operator()(
            const char* table_name, Str&& sql_str, Func&& do_after_creating_table) noexcept {
        try {
            if (not binary_search(sorted_tables, StringView{table_name})) {
                THROW("Table `", table_name, "` not found in the table list");
            }

            conn_.update(std::forward<Str>(sql_str));
            do_after_creating_table();

        } catch (const std::exception& e) {
            errlog("\033[31mFailed to create table `", table_name, "`\033[m - ", e.what());
            error = true;
        }
    }

    template <class Str>
    void operator()(const char* table_name, Str&& sql_str) noexcept {
        operator()(table_name, std::forward<Str>(sql_str), [] {});
    }
};

int main(int argc, char** argv) {
    stdlog.use(stdout);
    stdlog.label(false);
    errlog.label(false);

    auto cmd_options = parse_cmd_options(argc, argv);

    if (argc != 2) {
        help(argc > 0 ? argv[0] : nullptr);
        return 1;
    }

    auto db_config_path = concat(argv[1], "/.db.config");
    if (not path_exists(db_config_path)) {
        create_db_config(db_config_path);
    }

    mysql::Connection conn;
    try {
        // Get connection
        conn = sim::mysql::make_conn_with_credential_file(db_config_path);
        conn.update("SET foreign_key_checks=1"); // Just for sure

    } catch (const std::exception& e) {
        errlog("\033[31mFailed to connect to database, please edit or remove '", db_config_path,
                "' file and try again\033[m");
        ERRLOG_CATCH(e);
        return 4;
    }

    if (cmd_options.drop_tables) {
        for (auto&& table : reverse_view(tables)) {
            try {
                conn.update("DROP TABLE IF EXISTS `", table, '`');
            } catch (const std::exception& e) {
                errlog("\033[31mFailed to drop table '", table, "'\033[m - ", e.what());
                return 5;
            }
        }

        if (cmd_options.only_drop_tables) {
            return 0;
        }
    }

    TryToCreateTable try_to_create_table(conn);

    // clang-format off
    try_to_create_table("internal_files", concat(
        "CREATE TABLE IF NOT EXISTS `internal_files` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "PRIMARY KEY (id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::users::User;
    // clang-format off
    try_to_create_table("users", concat(
        "CREATE TABLE IF NOT EXISTS `users` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "`type` tinyint unsigned NOT NULL,"
            "`username` VARBINARY(", decltype(User::username)::max_len, ") NOT NULL,"
            "`first_name` VARBINARY(", decltype(User::first_name)::max_len, ") NOT NULL,"
            "`last_name` VARBINARY(", decltype(User::last_name)::max_len, ") NOT NULL,"
            "`email` VARBINARY(", decltype(User::email)::max_len, ") NOT NULL,"
            "`password_salt` BINARY(", decltype(User::password_salt)::max_len, ") NOT NULL,"
            "`password_hash` BINARY(", decltype(User::password_hash)::max_len, ") NOT NULL,"
            "PRIMARY KEY (id),"
            "UNIQUE KEY (username),"
            "KEY(type, id DESC)"
        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"),
            // clang-format on
            [&] {
                using sim::users::User;
                // Add sim root user
                auto [salt, hash] = sim::users::salt_and_hash_password("sim");
                decltype(User::type) sim_root_type = User::Type::ADMIN;
                conn.prepare("INSERT IGNORE INTO users(id, type, username, first_name, "
                             "last_name, "
                             "email, password_salt, password_hash) VALUES(?, ?, ?, ?, ?, ?, "
                             "?, ?)")
                        .bind_and_execute(sim::users::SIM_ROOT_UID, sim_root_type, "sim", "sim",
                                "sim", "sim@sim", salt, hash);
            });

    using sim::sessions::Session;
    // clang-format off
    try_to_create_table("sessions", concat(
        "CREATE TABLE IF NOT EXISTS `sessions` ("
            "`id` BINARY(", decltype(Session::id)::max_len, ") NOT NULL,"
            "`csrf_token` BINARY(", decltype(Session::csrf_token)::max_len, ") NOT NULL,"
            "`user_id` bigint unsigned NOT NULL,"
            "`data` blob NOT NULL,"
            "`user_agent` blob NOT NULL,"
            "`expires` datetime NOT NULL,"
            "PRIMARY KEY (id),"
            "KEY (user_id),"
            "KEY (expires),"
            "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin;"));
    // clang-format on

    using sim::problems::Problem;
    // clang-format off
    try_to_create_table("problems", concat(
        "CREATE TABLE IF NOT EXISTS `problems` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "`file_id` bigint unsigned NOT NULL,"
            "`type` tinyint unsigned NOT NULL,"
            "`name` VARBINARY(", decltype(Problem::name)::max_len, ") NOT NULL,"
            "`label` VARBINARY(", decltype(Problem::label)::max_len, ") NOT NULL,"
            "`simfile` mediumblob NOT NULL,"
            "`owner_id` bigint unsigned NULL,"
            "`created_at` datetime NOT NULL,"
            "`updated_at` datetime NOT NULL,"
            "PRIMARY KEY (id),"
            "KEY (owner_id, id),"
            "KEY (owner_id, type, id),"
            "KEY (type, id),"
            "FOREIGN KEY (file_id) REFERENCES internal_files(id) ON DELETE CASCADE,"
            "FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE SET NULL"
        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::problem_tags::ProblemTag;
    // clang-format off
    try_to_create_table("problem_tags", concat(
        "CREATE TABLE IF NOT EXISTS `problem_tags` ("
            "`problem_id` bigint unsigned NOT NULL,"
            "`name` VARBINARY(", decltype(ProblemTag::name)::max_len, ") NOT NULL,"
            "`is_hidden` BOOLEAN NOT NULL,"
            "PRIMARY KEY (problem_id, is_hidden, name),"
            "KEY (name, problem_id),"
            "FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::contests::Contest;
    // clang-format off
    try_to_create_table("contests",
        concat("CREATE TABLE IF NOT EXISTS `contests` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "`name` VARBINARY(", decltype(Contest::name)::max_len, ") NOT NULL,"
            "`is_public` BOOLEAN NOT NULL DEFAULT FALSE,"
            "PRIMARY KEY (id),"
            "KEY (is_public, id)"
        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::contest_rounds::ContestRound;
    // clang-format off
    try_to_create_table("contest_rounds",
        concat("CREATE TABLE IF NOT EXISTS `contest_rounds` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "`contest_id` bigint unsigned NOT NULL,"
            "`name` VARBINARY(", decltype(ContestRound::name)::max_len, ") NOT NULL,"
            "`item` bigint unsigned NOT NULL,"
            "`begins` BINARY(", decltype(ContestRound::begins)::max_len, ") NOT NULL,"
            "`ends` BINARY(", decltype(ContestRound::ends)::max_len, ") NOT NULL,"
            "`full_results` BINARY(", decltype(ContestRound::full_results)::max_len, ") NOT NULL,"
            "`ranking_exposure` BINARY(", decltype(ContestRound::ranking_exposure)::max_len, ") NOT NULL,"
            "PRIMARY KEY (id),"
            "KEY (contest_id, ranking_exposure),"
            "KEY (contest_id, begins),"
            "UNIQUE (contest_id, item),"
            "FOREIGN KEY (contest_id) REFERENCES contests(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::contest_problems::ContestProblem;
    // clang-format off
    try_to_create_table("contest_problems",
        concat("CREATE TABLE IF NOT EXISTS `contest_problems` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "`contest_round_id` bigint unsigned NOT NULL,"
            "`contest_id` bigint unsigned NOT NULL,"
            "`problem_id` bigint unsigned NOT NULL,"
            "`name` VARBINARY(", decltype(ContestProblem::name)::max_len, ") NOT NULL,"
            "`item` bigint unsigned NOT NULL,"
            "`method_of_choosing_final_submission` tinyint unsigned NOT NULL,"
            "`score_revealing` tinyint unsigned NOT NULL,"
            "PRIMARY KEY (id),"
            "UNIQUE (contest_round_id, item),"
            "KEY (contest_id),"
            "KEY (problem_id, id),"
            "FOREIGN KEY (contest_id) REFERENCES contests(id) ON DELETE CASCADE,"
            "FOREIGN KEY (contest_round_id) REFERENCES contest_rounds(id) ON DELETE CASCADE,"
            "FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE RESTRICT"
        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::contest_users::ContestUser;
    // clang-format off
    try_to_create_table("contest_users", concat(
        "CREATE TABLE IF NOT EXISTS `contest_users` ("
            "`user_id` bigint unsigned NOT NULL,"
            "`contest_id` bigint unsigned NOT NULL,"
            "`mode` tinyint unsigned NOT NULL DEFAULT ", EnumVal(ContestUser::Mode::CONTESTANT).to_int(), ","
            "PRIMARY KEY (user_id, contest_id),"
            "KEY (contest_id, user_id),"
            "KEY (contest_id, mode, user_id),"
            "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,"
            "FOREIGN KEY (contest_id) REFERENCES contests(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::contest_files::ContestFile;
    // clang-format off
    try_to_create_table("contest_files", concat(
        "CREATE TABLE IF NOT EXISTS `contest_files` ("
            "`id` BINARY(", decltype(ContestFile::id)::max_len, ") NOT NULL,"
            "`file_id` bigint unsigned NOT NULL,"
            "`contest_id` bigint unsigned NOT NULL,"
            "`name` VARBINARY(", decltype(ContestFile::name)::max_len, ") NOT NULL,"
            "`description` VARBINARY(", decltype(ContestFile::description)::max_len, ") NOT NULL,"
            "`file_size` bigint unsigned NOT NULL,"
            "`modified` datetime NOT NULL,"
            "`creator` bigint unsigned NULL,"
            "PRIMARY KEY (id),"
            "KEY (contest_id, modified),"
            "FOREIGN KEY (file_id) REFERENCES internal_files(id) ON DELETE CASCADE,"
            "FOREIGN KEY (contest_id) REFERENCES contests(id) ON DELETE CASCADE,"
            "FOREIGN KEY (creator) REFERENCES users(id) ON DELETE SET NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    using sim::contest_entry_tokens::ContestEntryToken;
    // clang-format off
    try_to_create_table("contest_entry_tokens",
        concat("CREATE TABLE IF NOT EXISTS `contest_entry_tokens` ("
            "`token` BINARY(", decltype(ContestEntryToken::token)::max_len, ") NOT NULL,"
            "`contest_id` bigint unsigned NOT NULL,"
            "`short_token` BINARY(", decltype(ContestEntryToken::short_token)::value_type::max_len, ") NULL,"
            "`short_token_expiration` datetime NULL,"
            "PRIMARY KEY (token),"
            "UNIQUE KEY (contest_id),"
            "UNIQUE KEY (short_token),"
            "FOREIGN KEY (contest_id) REFERENCES contests(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    // clang-format off
    try_to_create_table("submissions",
        "CREATE TABLE IF NOT EXISTS `submissions` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "`file_id` bigint unsigned NOT NULL,"
            "`owner` bigint unsigned NULL,"
            "`problem_id` bigint unsigned NOT NULL,"
            "`contest_problem_id` bigint unsigned NULL,"
            "`contest_round_id` bigint unsigned NULL,"
            "`contest_id` bigint unsigned NULL,"
            "`type` tinyint unsigned NOT NULL,"
            "`language` tinyint unsigned NOT NULL,"
            "`final_candidate` BOOLEAN NOT NULL DEFAULT FALSE,"
            "`problem_final` BOOLEAN NOT NULL DEFAULT FALSE,"
            "`contest_final` BOOLEAN NOT NULL DEFAULT FALSE,"
            // Used to color problems in the problem view
            "`contest_initial_final` BOOLEAN NOT NULL DEFAULT FALSE,"
            "`initial_status` tinyint unsigned NOT NULL,"
            "`full_status` tinyint unsigned NOT NULL,"
            "`submit_time` datetime NOT NULL,"
            "`score` bigint NULL DEFAULT NULL,"
            "`last_judgment` datetime NOT NULL,"
            "`initial_report` mediumblob NOT NULL,"
            "`final_report` mediumblob NOT NULL,"
            "PRIMARY KEY (id),"
            /* All needed by submissions API */
            // With owner
            "KEY (owner, id),"
            "KEY (owner, type, id),"
            "KEY (owner, problem_id, id),"
            "KEY (owner, contest_problem_id, id),"
            "KEY (owner, contest_round_id, id),"
            "KEY (owner, contest_id, id),"
            "KEY (owner, contest_final, id),"
            "KEY (owner, problem_final, id),"
            "KEY (owner, problem_id, type, id),"
            "KEY (owner, problem_id, problem_final),"
            "KEY (owner, contest_problem_id, type, id),"
            "KEY (owner, contest_problem_id, contest_final),"
            "KEY (owner, contest_round_id, type, id),"
            "KEY (owner, contest_round_id, contest_final, id),"
            "KEY (owner, contest_id, type, id),"
            "KEY (owner, contest_id, contest_final, id),"
            // Without owner
            "KEY (type, id),"
            "KEY (problem_id, id),"
            "KEY (contest_problem_id, id),"
            "KEY (contest_round_id, id),"
            "KEY (contest_id, id),"
            "KEY (problem_id, type, id),"
            "KEY (contest_problem_id, type, id),"
            "KEY (contest_round_id, type, id),"
            "KEY (contest_id, type, id),"
            // Needed to efficiently select final submission
            "KEY final1 (final_candidate, owner, contest_problem_id, id),"
            "KEY final2 (final_candidate, owner, contest_problem_id, score, full_status, id),"
            "KEY final3 (final_candidate, owner, problem_id, score, full_status, id),"
            // Needed to efficiently query contest view coloring
            "KEY initial_final (owner, contest_problem_id, contest_initial_final),"
            // Needed to efficiently update contest view coloring
            //   final = last compiling: final1
            //   no revealing and final = best submission:
            "KEY initial_final2 (final_candidate, owner, contest_problem_id, initial_status, id),"
            //   revealing score and final = best submission:
            "KEY initial_final3 (final_candidate, owner, contest_problem_id, score, initial_status, id),"
            // Foreign keys
            "FOREIGN KEY (file_id) REFERENCES internal_files(id) ON DELETE CASCADE,"
            "FOREIGN KEY (owner) REFERENCES users(id) ON DELETE CASCADE,"
            "FOREIGN KEY (problem_id) REFERENCES problems(id) ON DELETE CASCADE,"
            "FOREIGN KEY (contest_problem_id) REFERENCES contest_problems(id) ON DELETE CASCADE,"
            "FOREIGN KEY (contest_round_id) REFERENCES contest_rounds(id) ON DELETE CASCADE,"
            "FOREIGN KEY (contest_id) REFERENCES contests(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin");
    // clang-format on

    // clang-format off
    try_to_create_table("jobs", concat(
        "CREATE TABLE IF NOT EXISTS `jobs` ("
            "`id` bigint unsigned NOT NULL AUTO_INCREMENT,"
            "`file_id` bigint unsigned NULL DEFAULT NULL,"
            "`tmp_file_id` bigint unsigned NULL DEFAULT NULL,"
            "`creator` bigint unsigned NULL,"
            "`type` tinyint unsigned NOT NULL,"
            "`priority` tinyint unsigned NOT NULL,"
            "`status` tinyint unsigned NOT NULL,"
            "`added` datetime NOT NULL,"
            "`aux_id` bigint unsigned DEFAULT NULL,"
            "`info` blob NOT NULL,"
            "`data` mediumblob NOT NULL,"
            "PRIMARY KEY (id),"
            "KEY (status, priority DESC, id), "
            "KEY (type, aux_id, id DESC),"
            "KEY (aux_id, id DESC),"
            "KEY (creator, id DESC),"
            "KEY (creator, type, aux_id, id DESC),"
            "KEY (creator, aux_id, id DESC)"
            // Foreign keys cannot be used as we want to preserve information
            // about who created job and what the job was doing specifically
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));
    // clang-format on

    if (try_to_create_table.error) {
        return 7;
    }

    return 0;
}
