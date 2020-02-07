#include <sim/constants.h>
#include <sim/mysql.h>
#include <simlib/filesystem.h>
#include <simlib/process.h>
#include <simlib/sim/problem_package.h>
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

int main2(int argc, char** argv) {
	if (argc != 1) {
		help(argc > 0 ? argv[0] : nullptr);
		return 1;
	}

	chdirToExecDir();

#define MYSQL_CNF ".mysql.cnf"
	FileRemover mysql_cnf_guard;

	// Get connection
	auto conn = MySQL::make_conn_with_credential_file(".db.config");

	FileDescriptor fd {MYSQL_CNF, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
	                   S_0600};
	if (fd == -1) {
		errlog("Failed to open file `" MYSQL_CNF "`: open()", errmsg());
		return 1;
	}

	mysql_cnf_guard.reset(MYSQL_CNF);
	writeAll_throw(fd, intentionalUnsafeStringView(
	                      concat("[client]\nuser=", conn.impl()->user,
	                             "\npassword=", conn.impl()->passwd,
	                             "\nuser=", conn.impl()->user, "\n")));

	auto run_command = [](vector<string> args) {
		auto es = Spawner::run(args[0], args);
		if (es.si.code != CLD_EXITED or es.si.status != 0) {
			errlog(args[0], " failed: ", es.message);
			exit(1);
		}
	};

	// Remove temporary internal files that were not removed (e.g. a problem
	// adding job was canceled between the first and second stage while it was
	// pending)
	{
		using namespace std::chrono;
		using namespace std::chrono_literals;

		auto transaction = conn.start_transaction();
		auto stmt = conn.prepare(
		   "SELECT tmp_file_id FROM jobs "
		   "WHERE tmp_file_id IS NOT NULL AND status IN (" JSTATUS_DONE_STR
		   "," JSTATUS_FAILED_STR "," JSTATUS_CANCELED_STR ")");
		stmt.bindAndExecute();
		uint64_t tmp_file_id;
		stmt.res_bind_all(tmp_file_id);

		auto deleter = conn.prepare("DELETE FROM internal_files WHERE id=?");
		// Remove jobs temporary internal files
		while (stmt.next()) {
			auto file_path = internal_file_path(tmp_file_id);
			if (access(file_path, F_OK) == 0 and
			    system_clock::now() - get_modification_time(file_path) > 2h) {
				deleter.bindAndExecute(tmp_file_id);
				(void)unlink(file_path);
			}
		}

		// Remove internal files that do not have an entry in internal_files
		sim::PackageContents fc;
		fc.load_from_directory(INTERNAL_FILES_DIR);
		AVLDictSet<std::string> orphaned_files;
		fc.for_each_with_prefix("", [&](StringView file) {
			orphaned_files.emplace(file.to_string());
		});

		stmt = conn.prepare("SELECT id FROM internal_files");
		stmt.bindAndExecute();
		InplaceBuff<32> file_id;
		stmt.res_bind_all(file_id);
		while (stmt.next())
			orphaned_files.erase(file_id);

		// Remove orphaned files that are older than 2h (not to delete files
		// that are just created but not committed)
		orphaned_files.for_each([&](std::string const& file) {
			struct stat64 st;
			auto file_path = concat(INTERNAL_FILES_DIR, file);
			if (stat64(file_path.to_cstr().data(), &st)) {
				if (errno == ENOENT)
					return;

				THROW("stat64", errmsg());
			}

			if (system_clock::now() - get_modification_time(st) > 2h) {
				stdlog("Deleting: ", file);
				(void)unlink(file_path);
			}
		});

		transaction.commit();
	}

	FileRemover mysql_dump_guard {"dump.sql"};
	run_command({
	   "mysqldump",
	   "--defaults-file=" MYSQL_CNF,
	   "--result-file=dump.sql",
	   "--single-transaction",
	   conn.impl()->db,
	});

	run_command({"git", "init"});
	run_command({"git", "config", "--local", "user.name", "Sim backuper"});
	run_command({"git", "add", "--verbose", "static"});
	run_command({"git", "add", "--verbose", "dump.sql"});
	run_command({"git", "add", "--verbose", "internal_files/"});
	run_command({"git", "add", "--verbose", "logs/"});
	run_command({"git", "commit", "-m", concat_tostr("Backup ", mysql_date())});

	return 0;
}

int main(int argc, char** argv) {
	try {
		return main2(argc, argv);

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		return 1;
	}
}
