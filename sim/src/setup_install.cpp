#include "include/debug.h"
#include "include/sha.h"
#include "include/db.h"
#include "include/string.h"
#include "include/memory.h"

#include <cstring>

#include <cppconn/prepared_statement.h>

using std::string;

int main(int argc, char *argv[]) {
	if (argc < 2) {
		eprintf("Usage: setup_install INSTALL_DIR\n");
		return 1;
	}

	string db_config = argv[1];
	db_config += "/db.config";
	db_config = abspath(db_config, 0, string::npos, db_config[0] == '/' ? "/" : ".");

	E("db_config: '%s'\n", db_config.c_str());

	FILE *conf = fopen(db_config.c_str(), "r");
	if (conf == NULL) {
		eprintf("Cannot open file: '%s'\n", db_config.c_str());
		return 2;
	}

	// Get password
	size_t x1 = 0, x2 = 0, x3 = 0, x4 = 0;
	char *user = NULL, *password = NULL, *database = NULL, *host = NULL;
	if (getline(&user, &x1, conf) == -1 || getline(&password, &x2, conf) == -1 ||
			getline(&database, &x3, conf) == -1 ||
			getline(&host, &x4, conf) == -1) {
		eprintf("\e[31mFailed to get config\e[0m\n");
		fclose(conf);
		return 3;
	}
	fclose(conf);
	user[strlen(user)-1] = password[strlen(password)-1] = '\0';
	database[strlen(database)-1] = host[strlen(host)-1] = '\0';

	try {
		DB::Connection conn(host, user, password, database);

		delete[] user;
		delete[] password;
		delete[] database;
		delete[] host;

		UniquePtr<sql::Statement> stmt(conn->createStatement());

		try {
			stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `users` (\n"
					"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
					"`username` varchar(30) COLLATE utf8_bin NOT NULL,\n"
					"`first_name` varchar(60) COLLATE utf8_bin NOT NULL,\n"
					"`last_name` varchar(60) COLLATE utf8_bin NOT NULL,\n"
					"`email` varchar(60) COLLATE utf8_bin NOT NULL,\n"
					"`password` char(64) COLLATE utf8_bin NOT NULL,\n"
					"`type` enum('normal','teacher','admin') COLLATE utf8_bin NOT NULL DEFAULT 'normal',\n"
					"PRIMARY KEY (`id`),\n"
					"UNIQUE KEY `username` (`username`)\n"
				") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin\n");

				// Add default user sim with password sim
				UniquePtr<sql::PreparedStatement> pstmt(conn
					->prepareStatement("INSERT IGNORE INTO users (username, password, type) VALUES ('sim', ?, 'admin')"));
				pstmt->setString(1, sha256("sim"));
				pstmt->executeUpdate();
		} catch(...) {
			eprintf("\e[31mFailed to create table `users`\e[0m\n");
			return 5;
		}

		try {
			stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `session` (\n"
					"`id` char(10) COLLATE utf8_bin NOT NULL,\n"
					"`user_id` int unsigned NOT NULL,\n"
					"`data` text COLLATE utf8_bin NOT NULL,\n"
					"`ip` char(15) COLLATE utf8_bin NOT NULL,\n"
					"`user_agent` text COLLATE utf8_bin NOT NULL,\n"
					"`time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
					"PRIMARY KEY (`id`),\n"
					"KEY (`user_id`),\n"
					"KEY (`time`)\n"
				") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;\n");
		} catch(...) {
			eprintf("\e[31mFailed to create table `session`\e[0m\n");
			return 5;
		}

		try {
			stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `problems` (\n"
					"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
					"`access` enum('public', 'private') COLLATE utf8_bin NOT NULL DEFAULT 'private',\n"
					"`name` VARCHAR(128) NOT NULL,\n"
					"`owner` int unsigned NOT NULL,\n"
					"`added` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
					"PRIMARY KEY (`id`),\n"
					"KEY (`owner`)\n"
				") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");
		} catch(...) {
			eprintf("\e[31mFailed to create table `problems`\e[0m\n");
			return 5;
		}

		try {
			stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `rounds` (\n"
					"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
					"`parent` int unsigned NULL DEFAULT NULL,\n"
					"`grandparent` int unsigned NULL DEFAULT NULL,\n"
					"`problem_id` int unsigned DEFAULT NULL,\n"
					"`access` enum('public', 'private') COLLATE utf8_bin NOT NULL DEFAULT 'private',\n"
					"`name` VARCHAR(128) NOT NULL,\n"
					"`owner` int unsigned NOT NULL,\n"
					"`item` int unsigned NOT NULL,\n"
					"`visible` BOOLEAN NOT NULL DEFAULT FALSE,\n"
					"`begins` timestamp NULL DEFAULT NULL,\n"
					"`full_results` timestamp NULL DEFAULT NULL,\n"
					"`ends` timestamp NULL DEFAULT NULL,\n"
					"PRIMARY KEY (`id`),\n"
					"KEY (`parent`, `visible`),\n"
					"KEY (`parent`, `begins`),\n"
					"KEY (`parent`, `access`),\n"
					"KEY (`grandparent`, `item`),\n"
					"KEY (`owner`)\n"
				") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");
		} catch(...) {
			eprintf("\e[31mFailed to create table `rounds`\e[0m\n");
			return 5;
		}

		try {
			stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `users_to_rounds` (\n"
					"`user_id` int unsigned NOT NULL,\n"
					"`round_id` int unsigned NOT NULL,\n"
					"PRIMARY KEY (`user_id`, `round_id`),\n"
					"KEY (`round_id`)\n"
				") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");
		} catch(...) {
			eprintf("\e[31mFailed to create table `users_to_rounds`\e[0m\n");
			return 5;
		}
	} catch(...) {
		eprintf("\e[31mFailed to connect to database\e[0m\n");
		return 4;
	}
	return 0;
}
