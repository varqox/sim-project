#include <sim/constants.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/filesystem.h>
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

	printf("Usage: %s [options] INSTALL_DIR\n", program_name);
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

constexpr array<meta::string, 12> tables {{
	{"contest_entry_tokens"},
	{"contest_problems"},
	{"contest_rounds"},
	{"contest_users"},
	{"contests"},
	{"files"},
	{"jobs"},
	{"problem_tags"},
	{"problems"},
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
	void operator()(const char* table_name, StringView query, Func&& f)
		noexcept
	{
		try {
			if (!binary_search(tables, StringView{table_name}))
				THROW("Table `", table_name, "` not found in the table list");

			conn_.update(query);
			f();

		} catch (const std::exception& e) {
			errlog("\033[31mFailed to create table `", table_name, "`\033[m - ",
				e.what());
			error = true;
		}
	}

	void operator()(const char* table_name, StringView query) noexcept {
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
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX);
		conn = MySQL::make_conn_with_credential_file(
			concat(argv[1], "/.db.config").to_cstr());

	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 4;
	}

	if (DROP_TABLES) {
		try {
			for (auto&& table : tables)
				conn.update(concat("DROP TABLE IF EXISTS `", table, '`'));

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
			"`username` VARBINARY(", USERNAME_MAX_LEN, ") NOT NULL,"
			"`first_name` VARBINARY(", USER_FIRST_NAME_MAX_LEN, ") NOT NULL,"
			"`last_name` VARBINARY(", USER_LAST_NAME_MAX_LEN, ") NOT NULL,"
			"`email` VARBINARY(", USER_EMAIL_MAX_LEN, ") NOT NULL,"
			"`salt` BINARY(", SALT_LEN, ") NOT NULL,"
			"`password` BINARY(", PASSWORD_HASH_LEN, ") NOT NULL,"
			"`type` tinyint(1) unsigned NOT NULL DEFAULT " UTYPE_NORMAL_STR ","
			"PRIMARY KEY (id),"
			"UNIQUE KEY (username),"
			"KEY(type, id DESC)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"),
		[&] {
			// Add default user sim with password sim
			char salt_bin[SALT_LEN >> 1];
			fillRandomly(salt_bin, sizeof(salt_bin));
			string salt = toHex(salt_bin, sizeof(salt_bin));

			auto stmt = conn.prepare(
				"INSERT IGNORE users (id, username, first_name, last_name, email, "
					"salt, password, type) "
				"VALUES (" SIM_ROOT_UID ", 'sim', 'sim', 'sim', 'sim@sim', ?, "
					"?, 0)");
			stmt.bindAndExecute(salt, sha3_512(salt + "sim"));
		});

	try_to_create_table("session",
		concat("CREATE TABLE IF NOT EXISTS `session` ("
			"`id` BINARY(", SESSION_ID_LEN, ") NOT NULL,"
			"`csrf_token` BINARY(", SESSION_CSRF_TOKEN_LEN, ") NOT NULL,"
			"`user_id` int unsigned NOT NULL,"
			"`data` blob NOT NULL,"
			"`ip` VARBINARY(", SESSION_IP_LEN, ") NOT NULL,"
			"`user_agent` blob NOT NULL,"
			"`expires` datetime NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (user_id),"
			"KEY (expires)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;"));

	try_to_create_table("problems",
		concat("CREATE TABLE IF NOT EXISTS `problems` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`type` TINYINT NOT NULL,"
			"`name` VARBINARY(", PROBLEM_NAME_MAX_LEN, ") NOT NULL,"
			"`label` VARBINARY(", PROBLEM_LABEL_MAX_LEN, ") NOT NULL,"
			"`simfile` mediumblob NOT NULL,"
			"`owner` int unsigned NOT NULL,"
			"`added` datetime NOT NULL,"
			"`last_edit` datetime NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (owner, id),"
			"KEY (type, id)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("problem_tags",
		concat("CREATE TABLE IF NOT EXISTS `problem_tags` ("
			"`problem_id` int unsigned NOT NULL,"
			"`tag` VARBINARY(", PROBLEM_TAG_MAX_LEN, ") NOT NULL,"
			"`hidden` BOOLEAN NOT NULL,"
			"PRIMARY KEY (problem_id, hidden, tag),"
			"KEY (tag, problem_id)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("contests",
		concat("CREATE TABLE IF NOT EXISTS `contests` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`name` VARBINARY(", CONTEST_NAME_MAX_LEN, ") NOT NULL,"
			"`is_public` BOOLEAN NOT NULL DEFAULT FALSE,"
			"PRIMARY KEY (id),"
			"KEY (is_public, id)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("contest_rounds",
		concat("CREATE TABLE IF NOT EXISTS `contest_rounds` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`contest_id` int unsigned NOT NULL,"
			"`name` VARBINARY(", CONTEST_ROUND_NAME_MAX_LEN, ") NOT NULL,"
			"`item` int unsigned NOT NULL,"
			"`begins` CHAR(", CONTEST_ROUND_DATETIME_LEN, ") NOT NULL,"
			"`ends` CHAR(", CONTEST_ROUND_DATETIME_LEN, ") NOT NULL,"
			"`full_results` CHAR(", CONTEST_ROUND_DATETIME_LEN, ") NOT NULL,"
			"`ranking_exposure` CHAR(", CONTEST_ROUND_DATETIME_LEN, ") NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (contest_id, ranking_exposure),"
			"KEY (contest_id, begins),"
			"UNIQUE (contest_id, item)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("contest_problems",
		concat("CREATE TABLE IF NOT EXISTS `contest_problems` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`contest_round_id` int unsigned NOT NULL,"
			"`contest_id` int unsigned NOT NULL,"
			"`problem_id` int unsigned NOT NULL,"
			"`name` VARBINARY(", CONTEST_PROBLEM_NAME_MAX_LEN, ") NOT NULL,"
			"`item` int unsigned NOT NULL,"
			"`final_selecting_method` TINYINT NOT NULL,"
			"`reveal_score` BOOLEAN NOT NULL,"
			"PRIMARY KEY (id),"
			"UNIQUE (contest_round_id, item),"
			"KEY (contest_id),"
			"KEY (problem_id, id)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("contest_users",
		"CREATE TABLE IF NOT EXISTS `contest_users` ("
			"`user_id` int unsigned NOT NULL,"
			"`contest_id` int unsigned NOT NULL,"
			"`mode` tinyint(1) unsigned NOT NULL DEFAULT " CU_MODE_CONTESTANT_STR ","
			"PRIMARY KEY (user_id, contest_id),"
			"KEY (contest_id, user_id),"
			"KEY (contest_id, mode, user_id)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	try_to_create_table("submissions",
		"CREATE TABLE IF NOT EXISTS `submissions` ("
			"`id` int unsigned NOT NULL AUTO_INCREMENT,"
			"`owner` int unsigned NULL,"
			"`problem_id` int unsigned NOT NULL,"
			"`contest_problem_id` int unsigned NULL,"
			"`contest_round_id` int unsigned NULL,"
			"`contest_id` int unsigned NULL,"
			"`type` TINYINT NOT NULL,"
			"`language` TINYINT NOT NULL,"
			"`final_candidate` BOOLEAN NOT NULL DEFAULT FALSE,"
			"`problem_final` BOOLEAN NOT NULL DEFAULT FALSE,"
			"`contest_final` BOOLEAN NOT NULL DEFAULT FALSE,"
			// Used to color problems in the problem view
			"`contest_initial_final` BOOLEAN NOT NULL DEFAULT FALSE,"
			"`initial_status` TINYINT NOT NULL,"
			"`full_status` TINYINT NOT NULL,"
			"`submit_time` datetime NOT NULL,"
			"`score` int NULL DEFAULT NULL,"
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
			// Needed to efficiently change type to/from FINAL
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
			"KEY initial_final3 (final_candidate, owner, contest_problem_id, score, initial_status, id)"
		") ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	try_to_create_table("jobs",
		concat("CREATE TABLE IF NOT EXISTS `jobs` ("
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
			"KEY (type, aux_id, id DESC),"
			"KEY (aux_id, id DESC),"
			"KEY (creator, id DESC),"
			"KEY (creator, type, aux_id, id DESC),"
			"KEY (creator, aux_id, id DESC)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("files",
		concat("CREATE TABLE IF NOT EXISTS `files` ("
			"`id` BINARY(", FILE_ID_LEN, ") NOT NULL,"
			"`contest_id` int unsigned NULL,"
			"`name` VARBINARY(", FILE_NAME_MAX_LEN, ") NOT NULL,"
			"`description` VARBINARY(", FILE_DESCRIPTION_MAX_LEN, ") "
				"NOT NULL,"
			"`file_size` bigint unsigned NOT NULL,"
			"`modified` datetime NOT NULL,"
			"PRIMARY KEY (id),"
			"KEY (contest_id, modified)"
		") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin"));

	try_to_create_table("contest_entry_tokens",
		concat("CREATE TABLE IF NOT EXISTS `contest_entry_tokens` ("
			"`token` BINARY(", CONTEST_ENTRY_TOKEN_LEN, ") NOT NULL,"
			"`contest_id` int unsigned NULL,"
			"`short_token` BINARY(", CONTEST_ENTRY_SHORT_TOKEN_LEN, ") NULL,"
			"`short_token_expiration` datetime NULL,"
			"PRIMARY KEY (token),"
			"UNIQUE KEY (contest_id),"
			"UNIQUE KEY (short_token)"
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
