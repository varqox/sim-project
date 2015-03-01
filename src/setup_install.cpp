#include "include/debug.h"
#include "include/sha.h"
#include "include/db.h"
#include "include/string.h"

#include <cstring>

using std::string;

int main(int argc, char *argv[]) {
	if (argc < 2) {
		eprintf("Usage: setup_install INSTALL_DIR\n");
		return 1;
	}

	string db_config = argv[1];
	db_config += "/db.config";
	db_config = abspath(db_config, 0, string::npos, db_config[0] == '/' ? "/" : ".");

	D(eprintf("db_config: '%s'\n", db_config.c_str());)

	FILE *conf = fopen(db_config.c_str(), "r");
	if (conf == NULL) {
		eprintf("Cannot open file: '%s'\n", db_config.c_str());
		return 2;
	}

	// Get password
	size_t x;
	char *user = NULL, *password = NULL, *database = NULL, *host = NULL;
	if (getline(&user, &x, conf) == -1 || getline(&password, &x, conf) == -1 ||
			getline(&database, &x, conf) == -1 ||
			getline(&host, &x, conf) == -1) {
		eprintf("Failed to get config\n");
		fclose(conf);
		return 3;
	}
	fclose(conf);
	user[strlen(user)-1] = password[strlen(password)-1] = '\0';
	database[strlen(database)-1] = host[strlen(host)-1] = '\0';

	DB::Connection conn(host, user, password, database);

	delete[] user;
	delete[] password;
	delete[] database;
	delete[] host;

	sql::Statement *stmt = conn.mysql()->createStatement();

	if (stmt->execute("CREATE TABLE IF NOT EXISTS `users` (\n"
				"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
				"`username` varchar(30) COLLATE utf8_bin NOT NULL,\n"
				"`first_name` varchar(60) COLLATE utf8_bin NOT NULL,\n"
				"`last_name` varchar(60) COLLATE utf8_bin NOT NULL,\n"
				"`email` varchar(60) COLLATE utf8_bin NOT NULL,\n"
				"`password` char(64) COLLATE utf8_bin NOT NULL,\n"
				"`type` enum('normal','teacher','admin') COLLATE utf8_bin NOT NULL DEFAULT 'normal',\n"
				"PRIMARY KEY (`id`),\n"
				"UNIQUE KEY `username` (`username`)\n"
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin\n")) {
		eprintf("Failed to create table `users`\n");
		return 4;
	}
	// stmt->execute("INSERT IGNORE INTO `users` (`id`) VALUES(1)\n");

	if (stmt->execute("CREATE TABLE IF NOT EXISTS `session` (\n"
				"`id` char(10) COLLATE utf8_bin NOT NULL,\n"
				"`user_id` int unsigned NOT NULL,\n"
				"`data` text COLLATE utf8_bin NOT NULL,\n"
				"`user_agent` text COLLATE utf8_bin NOT NULL,\n"
				"`time` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
				"PRIMARY KEY (`id`),\n"
				"KEY (`user_id`),\n"
				"KEY (`time`)\n"
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;\n")) {
		eprintf("Failed to create table `session`\n");
		return 4;
	}

	delete stmt;
	return 0;
}
