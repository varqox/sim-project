#include <sim/constants.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/random.h>
#include <simlib/sha.h>
#include <simlib/utilities.h>

using std::array;
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

constexpr array<meta::string, 9> tables {{
	{"contests_users"},
	{"files"},
	{"job_queue"},
	{"problems"},
	{"problems_tags"},
	{"rounds"},
	{"session"},
	{"submissions"},
	{"users"},
}};

static_assert(meta::is_sorted(tables), "Needed for binary search");

struct TryToCreateTable {
	bool error = false;
	MySQL::Connection& conn_;

	explicit TryToCreateTable(MySQL::Connection& conn) : conn_(conn) {}

	template<class Func>
	void operator()(const char* table_name, const std::string& query, Func&& f)
		noexcept
	{
		try {
			if (!binary_search(tables, StringView{table_name}))
				THROW("Table `", table_name, "` not found in the table list");

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

	SQLite::Connection sqlite_db;
	MySQL::Connection conn;
	try {
		// Get connection
		sqlite_db = SQLite::Connection(
			StringBuff<PATH_MAX>{argv[1], "/" SQLITE_DB_FILE},
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
		conn = MySQL::createConnectionUsingPassFile(
			concat(argv[1], "/.db.config"));

	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 4;
	}

	if (DROP_TABLES) {
		try {
			for (auto&& table : tables)
				conn.executeUpdate(concat("DROP TABLE IF EXISTS `", table, '`'));

			if (ONLY_DROP_TABLES)
				return 0;

		} catch (const std::exception& e) {
			errlog("\033[31mFailed to drop tables\033[m - ", e.what());
			return 5;
		}
	}

	TryToCreateTable try_to_create_table(conn);

	try_to_create_table("users",
		concat("CREATE TABLE IF NOT EXISTS `users` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`username` varchar(", toStr(USERNAME_MAX_LEN), ") NOT NULL,"
			"`first_name` varchar(", toStr(USER_FIRST_NAME_MAX_LEN), ") NOT NULL,"
			"`last_name` varchar(", toStr(USER_LAST_NAME_MAX_LEN), ") NOT NULL,"
			"`email` varchar(", toStr(USER_EMAIL_MAX_LEN), ") NOT NULL,"
			"`salt` char(", toStr(SALT_LEN), ") NOT NULL,"
			"`password` char(", toStr(PASSWORD_HASH_LEN), ") NOT NULL,"
			"`type` tinyint(1) unsigned NOT NULL DEFAULT " UTYPE_NORMAL_STR ","
			"PRIMARY KEY (id),"
			"UNIQUE KEY (username)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"),
		[&] {
			// Add default user sim with password sim
			char salt_bin[SALT_LEN >> 1];
			fillRandomly(salt_bin, sizeof(salt_bin));
			string salt = toHex(salt_bin, sizeof(salt_bin));

			MySQL::Statement stmt(conn.prepare(
				"INSERT IGNORE users (id, username, first_name, last_name, email, "
					"salt, password, type) "
				"VALUES (" SIM_ROOT_UID ", 'sim', 'sim', 'sim', 'sim@sim', ?, "
					"?, 0)"));
			stmt.setString(1, salt);
			stmt.setString(2, sha3_512(salt + "sim"));
			stmt.executeUpdate();
		});

	try_to_create_table("session",
		concat("CREATE TABLE IF NOT EXISTS `session` ("
			"`id` char(", toStr(SESSION_ID_LEN), ") NOT NULL,"
			"`csrf_token` char(", toStr(SESSION_CSRF_TOKEN_LEN), ") NOT NULL,"
			"`user_id` int unsigned NOT NULL,"
			"`data` blob NOT NULL,"
			"`ip` char(", toStr(SESSION_IP_LEN), ") NOT NULL,"
			"`user_agent` blob NOT NULL,"
			"`time` datetime NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (user_id),"
			"KEY (time)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;"));

	try_to_create_table("problems",
		concat("CREATE TABLE IF NOT EXISTS `problems` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`type` TINYINT NOT NULL,"
			"`name` VARCHAR(", toStr(PROBLEM_NAME_MAX_LEN), ") NOT NULL,"
			"`label` VARCHAR(", toStr(PROBLEM_LABEL_MAX_LEN), ") NOT NULL,"
			"`simfile` mediumblob NOT NULL,"
			"`owner` int unsigned NOT NULL,"
			"`added` datetime NOT NULL,"
			"`last_edit` datetime NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (owner, id),"
			"KEY (type, id)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("problems_tags",
		concat("CREATE TABLE IF NOT EXISTS `problems_tags` ("
			"`problem_id` int unsigned NOT NULL,"
			"`tag` VARCHAR(", toStr(PROBLEM_TAG_MAX_LEN), ") NOT NULL,"
			"PRIMARY KEY (problem_id, tag),"
			"KEY (tag)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("rounds",
		concat("CREATE TABLE IF NOT EXISTS `rounds` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`parent` int unsigned NULL DEFAULT NULL,"
			"`grandparent` int unsigned NULL DEFAULT NULL,"
			"`problem_id` int unsigned DEFAULT NULL,"
			"`name` VARCHAR(", toStr(ROUND_NAME_MAX_LEN), ") NOT NULL,"
			"`owner` int unsigned NOT NULL,"
			"`item` int unsigned NOT NULL,"
			"`is_public` BOOLEAN NOT NULL DEFAULT FALSE,"
			"`visible` BOOLEAN NOT NULL DEFAULT FALSE,"
			"`show_ranking` BOOLEAN NOT NULL DEFAULT FALSE,"
			"`begins` datetime NULL DEFAULT NULL,"
			"`full_results` datetime NULL DEFAULT NULL,"
			"`ends` datetime NULL DEFAULT NULL,"
			"PRIMARY KEY (id),"
			"KEY (parent, visible),"
			"KEY (parent, begins),"
			"KEY (parent, is_public),"
			"KEY (grandparent, item),"
			"KEY (owner)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("contests_users",
		"CREATE TABLE IF NOT EXISTS `contests_users` ("
			"`user_id` int unsigned NOT NULL,"
			"`contest_id` int unsigned NOT NULL,"
			"`mode` tinyint(1) unsigned NOT NULL DEFAULT " CU_MODE_CONTESTANT_STR ","
			"PRIMARY KEY (user_id, contest_id),"
			"KEY (contest_id)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	try_to_create_table("submissions",
		"CREATE TABLE IF NOT EXISTS `submissions` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`user_id` int unsigned NOT NULL,"
			"`problem_id` int unsigned NOT NULL,"
			"`round_id` int unsigned NULL,"
			"`parent_round_id` int unsigned NULL,"
			"`contest_round_id` int unsigned NULL,"
			"`type` TINYINT NOT NULL,"
			"`status` TINYINT NOT NULL,"
			"`submit_time` datetime NOT NULL,"
			"`score` int NULL DEFAULT NULL,"
			"`last_judgment` datetime NOT NULL,"
			"`initial_report` mediumblob NOT NULL,"
			"`final_report` mediumblob NOT NULL,"
			"PRIMARY KEY (id),"
			// Update type, delete account
			"KEY (user_id, round_id, type, status, id),"
			// Problemset::submissions - view - all
			"KEY (problem_id, id),"
			"KEY (problem_id, user_id, id),"
			// Problemset::submissions() - view by type
			"KEY (problem_id, type, id),"
			"KEY (problem_id, user_id, type, id),"
			// Contest::submissions() - view all
			"KEY (round_id, id),"
			"KEY (round_id, user_id, id),"
			"KEY (parent_round_id, id),"
			"KEY (parent_round_id, user_id, id),"
			"KEY (contest_round_id, id),"
			"KEY (contest_round_id, user_id, id),"
			// Contest::submissions() - view by type
			"KEY (round_id, type, id),"
			"KEY (round_id, user_id, type, id),"
			"KEY (parent_round_id, type, id),"
			"KEY (parent_round_id, user_id, type, id),"
			"KEY (contest_round_id, type, id),"
			"KEY (contest_round_id, user_id, type, id)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	try_to_create_table("job_queue",
		concat("CREATE TABLE IF NOT EXISTS `job_queue` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`creator` int unsigned NULL,"
			"`type` TINYINT NOT NULL,"
			"`priority` TINYINT NOT NULL,"
			"`status` TINYINT NOT NULL,"
			"`added` datetime NOT NULL,"
			"`aux_id` int unsigned DEFAULT NULL,"
			"`info` blob NOT NULL,"
			"`data` mediumblob NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (status, priority DESC, id), "
			"KEY (type, aux_id),"
			"KEY (creator, id)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("files",
		concat("CREATE TABLE IF NOT EXISTS `files` ("
			"`id` char(", toStr(FILE_ID_LEN), ") NOT NULL,"
			"`round_id` int unsigned NULL,"
			"`name` VARCHAR(", toStr(FILE_NAME_MAX_LEN), ") NOT NULL,"
			"`description` VARCHAR(", toStr(FILE_DESCRIPTION_MAX_LEN), ") "
				"NOT NULL,"
			"`file_size` bigint unsigned NOT NULL,"
			"`modified` datetime NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (round_id, modified)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	// Create SQLite tables
	try {
		sqlite_db.execute("PRAGMA journal_mode=WAL");
		sqlite_db.execute("CREATE VIRTUAL TABLE IF NOT EXISTS problems"
			" USING fts5(type, name, label)");

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 6;
	}

	if (try_to_create_table.error)
		return 7;

	return 0;
}
