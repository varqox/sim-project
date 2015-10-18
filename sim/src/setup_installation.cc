#include "include/db.h"
#include "simlib/include/debug.h"
#include "simlib/include/sha.h"
#include "simlib/include/string.h"

#include <cppconn/prepared_statement.h>

using std::string;

int main(int argc, char **argv) {
	if (argc < 2) {
		eprintf("Usage: %s INSTALL_DIR\n",
			(argc > 0 ? argv[0] : "setup-installationation"));
		return 1;
	}

	DB::Connection conn;
	try {
		// Get connection
		conn = DB::createConnectionUsingPassFile(
			concat(argv[1], "/.db.config"));

	} catch (const std::exception& e) {
		eprintf("\e[31mFailed to connect to database\e[m - %s\n", e.what());
		return 4;
	}

	bool error = false;
	UniquePtr<sql::Statement> stmt(conn->createStatement());

	// users
	try {
		stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `users` (\n"
				"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
				"`username` varchar(30) COLLATE utf8_bin NOT NULL,\n"
				"`first_name` varchar(60) COLLATE utf8_bin NOT NULL,\n"
				"`last_name` varchar(60) COLLATE utf8_bin NOT NULL,\n"
				"`email` varchar(60) COLLATE utf8_bin NOT NULL,\n"
				"`password` char(64) COLLATE utf8_bin NOT NULL,\n"
				"`type` tinyint(1) unsigned NOT NULL DEFAULT 2,\n"
				"PRIMARY KEY (`id`),\n"
				"UNIQUE KEY `username` (`username`)\n"
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin\n");

			// Add default user sim with password sim
			UniquePtr<sql::PreparedStatement> pstmt(conn->
				prepareStatement("INSERT IGNORE INTO users "
					"(username, password, type) VALUES ('sim', ?, 0)"));
			pstmt->setString(1, sha256("sim"));
			pstmt->executeUpdate();

	} catch (const std::exception& e) {
		eprintf("\e[31mFailed to create table `users`\e[m - %s\n", e.what());
		error = true;
	}

	// session
	try {
		stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `session` (\n"
				"`id` char(10) COLLATE utf8_bin NOT NULL,\n"
				"`user_id` int unsigned NOT NULL,\n"
				"`data` text COLLATE utf8_bin NOT NULL,\n"
				"`ip` char(15) COLLATE utf8_bin NOT NULL,\n"
				"`user_agent` text COLLATE utf8_bin NOT NULL,\n"
				"`time` datetime NOT NULL,\n"
				"PRIMARY KEY (`id`),\n"
				"KEY (`user_id`),\n"
				"KEY (`time`)\n"
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin;\n");

	} catch (const std::exception& e) {
		eprintf("\e[31mFailed to create table `session`\e[m - %s\n", e.what());
		error = true;
	}

	// problems
	try {
		stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `problems` (\n"
				"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
				"`access` enum('public', 'private') COLLATE utf8_bin NOT NULL DEFAULT 'private',\n"
				"`name` VARCHAR(128) NOT NULL,\n"
				"`tag` CHAR(4) NOT NULL,\n"
				"`owner` int unsigned NOT NULL,\n"
				"`added` datetime NOT NULL,\n"
				"PRIMARY KEY (`id`),\n"
				"KEY (`owner`)\n"
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	} catch (const std::exception& e) {
		eprintf("\e[31mFailed to create table `problems`\e[m - %s\n", e.what());
		error = true;
	}

	// rounds
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
				"`show_ranking` BOOLEAN NOT NULL DEFAULT FALSE,\n"
				"`begins` datetime NULL DEFAULT NULL,\n"
				"`full_results` datetime NULL DEFAULT NULL,\n"
				"`ends` datetime NULL DEFAULT NULL,\n"
				"PRIMARY KEY (`id`),\n"
				"KEY (`parent`, `visible`),\n"
				"KEY (`parent`, `begins`),\n"
				"KEY (`parent`, `access`),\n"
				"KEY (`grandparent`, `item`),\n"
				"KEY (`owner`)\n"
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	} catch (const std::exception& e) {
		eprintf("\e[31mFailed to create table `rounds`\e[m - %s\n", e.what());
		error = true;
	}

	// users_to_contests
	try {
		stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `users_to_contests` (\n"
				"`user_id` int unsigned NOT NULL,\n"
				"`contest_id` int unsigned NOT NULL,\n"
				"PRIMARY KEY (`user_id`, `contest_id`),\n"
				"KEY (`contest_id`)\n"
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	} catch (const std::exception& e) {
		eprintf("\e[31mFailed to create table `users_to_rounds`\e[m - %s\n",
			e.what());
		error = true;
	}

	// submissions
	try {
		stmt->executeUpdate("CREATE TABLE IF NOT EXISTS `submissions` (\n"
				"`id` int unsigned NOT NULL AUTO_INCREMENT,\n"
				"`user_id` int unsigned NOT NULL,\n"
				"`problem_id` int unsigned NOT NULL,\n"
				"`round_id` int unsigned NOT NULL,\n"
				"`parent_round_id` int unsigned NOT NULL,\n"
				"`contest_round_id` int unsigned NOT NULL,\n"
				"`final` BOOLEAN NOT NULL DEFAULT FALSE,\n"
				"`submit_time` datetime NOT NULL,\n"
				"`status` enum('ok','error','c_error','judge_error','waiting') NULL DEFAULT NULL COLLATE utf8_bin,\n"
				"`score` int NULL DEFAULT NULL,\n"
				"`queued` datetime NOT NULL,\n"
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
			") ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_bin");

	} catch (const std::exception& e) {
		eprintf("\e[31mFailed to create table `submissions`\e[m - %s\n",
			e.what());
		error = true;
	}

	if (error)
		return 5;

	return 0;
}
