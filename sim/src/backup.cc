#include <sim/constants.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>
#include <simlib/spawner.h>
#include <simlib/time.h>

using std::string;
using std::vector;


/**
 * @brief Displays help
 */
static void help(const char* program_name) {
	if (program_name == nullptr)
		program_name = "backup";

	printf("Usage: %s [options]\n", program_name);
	puts("Make a backup of solutions and database contents");
}

// Backup the SQLite database
static void backup_sqlite_db(CStringView backup_file) {
	SQLite::Connection sqlite_db {SQLITE_DB_FILE, SQLITE_OPEN_READONLY};
	SQLite::Connection sqlite_backup {backup_file, SQLITE_OPEN_READWRITE |
		SQLITE_OPEN_CREATE};
	sqlite_backup.execute("PRAGMA journal_mode=WAL");

	// Initialize backup
	sqlite3_backup* backuper = sqlite3_backup_init(sqlite_backup, "main",
		sqlite_db, "main");
	if (!backuper)
		THROW_SQLITE_ERROR(sqlite_backup, "sqlite3_backup_init()");

	for (;;) {
		int rc = sqlite3_backup_step(backuper, 164);

		if (rc != SQLITE_OK and rc != SQLITE_BUSY and rc != SQLITE_LOCKED)
			break;

		sqlite3_sleep(250); // 250 ms
	}

	if (sqlite3_backup_finish(backuper))
		THROW_SQLITE_ERROR(sqlite_backup, "sqlite3_backup_step()");
}

int main2(int argc, char**argv) {
	if(argc != 1) {
		help(argc > 0 ? argv[0] : nullptr);
		return 1;
	}

	chdirToExecDir();

	#define MYSQL_CNF ".mysql.cnf"
	FileRemover mysql_cnf_guard;

	// Get connection
	auto conn = MySQL::makeConnWithCredFile(".db.config");

	FileDescriptor fd {MYSQL_CNF, O_WRONLY | O_CREAT | O_TRUNC, S_0600};
	if (fd == -1) {
		errlog("Failed to open file `" MYSQL_CNF "`: open()", error(errno));
		return 1;
	}

	mysql_cnf_guard.reset(MYSQL_CNF);
	writeAll_throw(fd, concat("[client]\n"
		"user=", conn.impl()->user, "\n"
		"password=", conn.impl()->passwd, "\n"
		"user=", conn.impl()->user, "\n"));

	auto run_command = [](vector<string> args) {
		auto es = Spawner::run(args[0], args);
		if (es.code) {
			auto tmplog = errlog(args[0], " failed with code ", toStr(es.code));
			if (es.message.size())
				tmplog(" because: ", es.message);

			exit(1);
		}
	};

	FileRemover mysql_dump_guard {"dump.sql"};
	run_command({
		"mysqldump",
		"--defaults-file=" MYSQL_CNF,
		"--result-file=dump.sql",
		"--extended-insert=FALSE",
		conn.impl()->db,
	});

	backup_sqlite_db(SQLITE_DB_FILE ".backup");

	run_command({"git", "init"});
	run_command({"git", "config", "--local", "user.name", "Sim backuper"});
	run_command({"git", "add", "solutions", SQLITE_DB_FILE ".backup",
		"dump.sql"});
	run_command({"git", "commit", "-m", concat("Backup ", date())});

	return 0;
}

int main(int argc, char **argv) {
	try {
		return main2(argc, argv);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}
}
