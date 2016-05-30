#include <sim/constants.h>
#include <sim/db.h>
#include <simlib/logger.h>
#include <simlib/random.h>
#include <simlib/sha.h>

using std::string;
using std::unique_ptr;

static bool DROP_TABLES = false, ONLY_DROP_TABLES = false;

/**
 * @brief Displays help
 */
static void help(const char* program_name) {
	if (program_name == nullptr)
		program_name = "setup-installation";

	printf("Usage: %s [options] INSTALL_DIR", program_name);
	puts("Setup database after SIM installation");
	puts("");
	puts("Options:");
	puts("  --drop-tables          Drop database tables before recreating them");
	puts("  -h, --help             Display this information");
	puts("  --only-drop-tables     Drop database tables and exit");
}

static void parseOptions(int &argc, char **argv) {
	int new_argc = 1;
	for (int i = 1; i < argc; ++i) {

		if (argv[i][0] == '-') {
			// Drop tables
			if (0 == strcmp(argv[i], "--drop-tables"))
				DROP_TABLES = true;

			// Help
			else if (0 == strcmp(argv[i], "-h") ||
				0 == strcmp(argv[i], "--help"))
			{
				help(argv[0]); // argv[0] is valid (argc > 1)
				exit(0);
			}

			// Drop tables
			else if (0 == strcmp(argv[i], "--only-drop-tables")) {
				DROP_TABLES = true;
				ONLY_DROP_TABLES = true;
			}

			// Unknown
			else
				errlog("Unknown option: '", argv[i], '\'');

		} else
			argv[new_argc++] = argv[i];
	}

	argc = new_argc;
}

struct TryCreateTable {
	bool error = false;
	DB::Connection& conn_;

	explicit TryCreateTable(DB::Connection& conn) : conn_(conn) {}

	template<class Func>
	void operator()(const char* table_name, const std::string& query, Func&& f)
		noexcept
	{
		try {
			conn_.executeUpdate(query);
			f();
		} catch (const std::exception& e) {
			errlog("\033[31mFailed to create table `", table_name, "`\033[m - ",
				e.what());
			error = true;
		}
	}

	void operator()(const char* table_name, const string& query) noexcept {
		operator()(table_name, query, []{});
	}
};

int main(int argc, char **argv) {
	errlog.label(false);

	parseOptions(argc, argv);

	if(argc != 2) {
		help(argc > 0 ? argv[0] : nullptr);
		return 1;
	}

	DB::Connection conn;
	try {
		// Get connection
		conn = DB::createConnectionUsingPassFile(
			concat(argv[1], "/.db.config"));

	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 4;
	}

	if (DROP_TABLES) {
		try {
			conn.executeUpdate("DROP TABLE iF EXISTS users");
			conn.executeUpdate("DROP TABLE iF EXISTS session");
			conn.executeUpdate("DROP TABLE iF EXISTS problems");
			conn.executeUpdate("DROP TABLE iF EXISTS rounds");
			conn.executeUpdate("DROP TABLE iF EXISTS users_to_contests");
			conn.executeUpdate("DROP TABLE iF EXISTS submissions");
			conn.executeUpdate("DROP TABLE iF EXISTS files");

			if (ONLY_DROP_TABLES)
				return 0;

		} catch (const std::exception& e) {
			errlog("\033[31mFailed to drop tables\033[m - ", e.what());
			return 5;
		}
	}

	TryCreateTable tryCreateTable(conn);

	tryCreateTable("users",
		concat("CREATE TABLE IF NOT EXISTS `users` (\n"
			"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
			"`username` varchar(", toStr(USERNAME_MAX_LEN), ") NOT NULL,\n"
			"`first_name` varchar(", toStr(USER_FIRST_NAME_MAX_LEN), ") NOT NULL,\n"
			"`last_name` varchar(", toStr(USER_LAST_NAME_MAX_LEN), ") NOT NULL,\n"
			"`email` varchar(", toStr(USER_EMAIL_MAX_LEN), ") NOT NULL,\n"
			"`salt` char(", toStr(SALT_LEN), ") NOT NULL,\n"
			"`password` char(", toStr(PASSWORD_HASH_LEN), ") NOT NULL,\n"
			"`type` tinyint(1) unsigned NOT NULL DEFAULT " UTYPE_NORMAL_STR ",\n"
			"PRIMARY KEY (`id`),\n"
			"UNIQUE KEY `username` (`username`)\n"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin\n"),
		[&] {
			// Add default user sim with password sim
			char salt_bin[SALT_LEN >> 1];
			fillRandomly(salt_bin, sizeof(salt_bin));
			string salt = toHex(salt_bin, sizeof(salt_bin));

			DB::Statement stmt(conn.prepare(
				"INSERT IGNORE users (id, username, first_name, last_name, email, "
					"salt, password, type) "
				"VALUES (" SIM_ROOT_UID ", 'sim', 'sim', 'sim', 'sim@sim', ?, "
					"?, 0)"));
			stmt.setString(1, salt);
			stmt.setString(2, sha3_512(salt + "sim"));
			stmt.executeUpdate();
		});

	tryCreateTable("session",
		concat("CREATE TABLE IF NOT EXISTS `session` (\n"
			"`id` char(", toStr(SESSION_ID_LEN), ") NOT NULL,\n"
			"`user_id` int unsigned NOT NULL,\n"
			"`data` blob NOT NULL,\n"
			"`ip` char(", toStr(SESSION_IP_LEN), ") NOT NULL,\n"
			"`user_agent` blob NOT NULL,\n"
			"`time` datetime NOT NULL,\n"
			"PRIMARY KEY (`id`),\n"
			"KEY (`user_id`),\n"
			"KEY (`time`)\n"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;\n"));

	tryCreateTable("problems",
		concat("CREATE TABLE IF NOT EXISTS `problems` (\n"
			"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
			"`is_public` BOOLEAN NOT NULL DEFAULT FALSE,\n"
			"`name` VARCHAR(", toStr(PROBLEM_NAME_MAX_LEN), ") NOT NULL,\n"
			"`tag` CHAR(", toStr(PROBLEM_TAG_LEN), ") NOT NULL,\n"
			"`owner` int unsigned NOT NULL,\n"
			"`added` datetime NOT NULL,\n"
			"PRIMARY KEY (`id`),\n"
			"KEY (`owner`)\n"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	tryCreateTable("rounds",
		concat("CREATE TABLE IF NOT EXISTS `rounds` (\n"
			"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
			"`parent` int unsigned NULL DEFAULT NULL,\n"
			"`grandparent` int unsigned NULL DEFAULT NULL,\n"
			"`problem_id` int unsigned DEFAULT NULL,\n"
			"`name` VARCHAR(", toStr(ROUND_NAME_MAX_LEN), ") NOT NULL,\n"
			"`owner` int unsigned NOT NULL,\n"
			"`item` int unsigned NOT NULL,\n"
			"`is_public` BOOLEAN NOT NULL DEFAULT FALSE,\n"
			"`visible` BOOLEAN NOT NULL DEFAULT FALSE,\n"
			"`show_ranking` BOOLEAN NOT NULL DEFAULT FALSE,\n"
			"`begins` datetime NULL DEFAULT NULL,\n"
			"`full_results` datetime NULL DEFAULT NULL,\n"
			"`ends` datetime NULL DEFAULT NULL,\n"
			"PRIMARY KEY (`id`),\n"
			"KEY (`parent`, `visible`),\n"
			"KEY (`parent`, `begins`),\n"
			"KEY (`parent`, `is_public`),\n"
			"KEY (`grandparent`, `item`),\n"
			"KEY (`owner`)\n"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	tryCreateTable("users_to_contests",
		"CREATE TABLE IF NOT EXISTS `users_to_contests` (\n"
			"`user_id` int unsigned NOT NULL,\n"
			"`contest_id` int unsigned NOT NULL,\n"
			"PRIMARY KEY (`user_id`, `contest_id`),\n"
			"KEY (`contest_id`)\n"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	tryCreateTable("submissions",
		"CREATE TABLE IF NOT EXISTS `submissions` (\n"
			"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
			"`user_id` int unsigned NOT NULL,\n"
			"`problem_id` int unsigned NOT NULL,\n"
			"`round_id` int unsigned NOT NULL,\n"
			"`parent_round_id` int unsigned NOT NULL,\n"
			"`contest_round_id` int unsigned NOT NULL,\n"
			"`final` BOOLEAN NOT NULL DEFAULT FALSE,\n"
			"`submit_time` datetime NOT NULL,\n"
			"`status` enum('ok','error','c_error','judge_error','waiting') NULL DEFAULT NULL,\n"
			"`score` int NULL DEFAULT NULL,\n"
			"`queued` datetime NOT NULL,\n"
			"`initial_report` blob NOT NULL,\n"
			"`final_report` blob NOT NULL,\n"
			"PRIMARY KEY (id),\n"
			// Judge server
			"KEY (status, queued),\n"
			// Update final, delete account
			"KEY (user_id, round_id, status, id),\n"
			// Contest::submissions() - view all
			"KEY (round_id, id),\n"
			"KEY (round_id, user_id, id),\n"
			"KEY (parent_round_id, id),\n"
			"KEY (parent_round_id, user_id, id),\n"
			"KEY (contest_round_id, id),\n"
			"KEY (contest_round_id, user_id, id),\n"
			// Contest::submissions() - view only finals
			"KEY (round_id, final, id),\n"
			"KEY (round_id, user_id, final, id),\n"
			"KEY (parent_round_id, final, id),\n"
			"KEY (parent_round_id, user_id, final, id),\n"
			"KEY (contest_round_id, final, id),\n"
			"KEY (contest_round_id, user_id, final, id)\n"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	tryCreateTable("files",
		concat("CREATE TABLE IF NOT EXISTS `files` (\n"
			"`id` char(", toStr(FILE_ID_LEN), ") NOT NULL,\n"
			"`round_id` int unsigned NULL,\n"
			"`name` VARCHAR(", toStr(FILE_NAME_MAX_LEN), ") NOT NULL,\n"
			"`description` VARCHAR(", toStr(FILE_DESCRIPTION_MAX_LEN), ") "
				"NOT NULL,\n"
			"`file_size` bigint unsigned NOT NULL,\n"
			"`modified` datetime NOT NULL,\n"
			"PRIMARY KEY (id),\n"
			"KEY (round_id, modified)\n"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	if (tryCreateTable.error)
		return 6;

	return 0;
}