#include "web_server/users/api.hh"

#include <fcntl.h>
#include <iostream>
#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/contest_files/contest_file.hh>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/contests/contest.hh>
#include <sim/db/tables.hh>
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
    write_all_throw(
        fd,
        from_unsafe{concat(
            "user: ",
            ConfigFile::escape_string(user),
            "\npassword: ",
            ConfigFile::escape_string(password),
            "\ndb: ",
            ConfigFile::escape_string(database),
            "\nhost: ",
            ConfigFile::escape_string(host),
            '\n'
        )}
    );
}

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
        errlog(
            "\033[31mFailed to connect to database, please edit or remove '",
            db_config_path,
            "' file and try again\033[m"
        );
        ERRLOG_CATCH(e);
        return 4;
    }

    if (cmd_options.drop_tables) {
        for (auto&& table : reverse_view(sim::db::tables)) {
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

    return 0;
}
