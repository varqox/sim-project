#include <iostream>
#include <map>
#include <sim/constants.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <sim/submission.h>
#include <simlib/process.h>
#include <simlib/random.h>
#include <simlib/sha.h>
#include <simlib/spawner.h>
#include <simlib/utilities.h>

using std::array;
using std::cin;
using std::cout;
using std::endl;
using std::map;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;
using MySQL::bind_arg;

SQLite::Connection importer_db;
MySQL::Connection old_conn, new_conn;

string old_build;
string new_build;

namespace old {

enum class SubmissionType : uint8_t {
	NORMAL = 0,
	FINAL = 1,
	VOID = 2,
	IGNORED = 3,
	PROBLEM_SOLUTION = 4,
};

// Initial and final values may be combined, but special not
enum class SubmissionStatus : uint8_t {
	// Final
	OK = 1,
	WA = 2,
	TLE = 3,
	MLE = 4,
	RTE = 5,
	FINAL_MASK = 7,
	// Initial
	INITIAL_OK  = OK << 3,
	INITIAL_WA  = WA << 3,
	INITIAL_TLE = TLE << 3,
	INITIAL_MLE = MLE << 3,
	INITIAL_RTE = RTE << 3,
	INITIAL_MASK = FINAL_MASK << 3,
	// Special
	PENDING                   = (8 << 3) + 0,
	// Fatal
	COMPILATION_ERROR         = (8 << 3) + 1,
	CHECKER_COMPILATION_ERROR = (8 << 3) + 2,
	JUDGE_ERROR               = (8 << 3) + 3
};
DECLARE_ENUM_UNARY_OPERATOR(SubmissionStatus, ~)
DECLARE_ENUM_OPERATOR(SubmissionStatus, |)
DECLARE_ENUM_OPERATOR(SubmissionStatus, &)

} // namespace old

int main2(int argc, char **argv) {
	STACK_UNWINDING_MARK;

	stdlog.use(stdout);

	if (argc != 3) {
		errlog.label(false);
		errlog("You have to specify the path to the old sim installation as the first argument and the path to new installation as the second argument");
		return 1;
	}

	try {
		// Get connection
		old_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(old_build = argv[1], "/.db.config"));
		new_conn = MySQL::make_conn_with_credential_file(
			concat_tostr(new_build = argv[2], "/.db.config"));
		// importer_db = SQLite::Connection("importer.db",
		// 	SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

	} catch (const std::exception& e) {
		errlog("\033[31mFailed to connect to database\033[m - ", e.what());
		return 4;
	}

	new_conn.update("TRUNCATE submissions");
	auto stmt = old_conn.prepare("SELECT id, owner, problem_id, contest_problem_id, contest_round_id, contest_id, type, language, final_candidate, status, submit_time, score, last_judgment, initial_report, final_report FROM submissions");

	InplaceBuff<64> id, owner, problem_id, contest_problem_id, contest_round_id, contest_id, language, final_candidate, submit_time, score, last_judgment, initial_report, final_report;
	uint type, status;
	my_bool isnull_owner, isnull_contest_problem_id, isnull_contest_round_id, isnull_contest_id, isnull_score;

	stmt.res_bind_all(id, bind_arg(owner, isnull_owner), problem_id, bind_arg(contest_problem_id, isnull_contest_problem_id), bind_arg(contest_round_id, isnull_contest_round_id), bind_arg(contest_id, isnull_contest_id), type, language, final_candidate, status, submit_time, bind_arg(score, isnull_score), last_judgment, initial_report, final_report);

	stmt.bindAndExecute();
	auto inserter = new_conn.prepare("INSERT INTO submissions (id, owner, problem_id, contest_problem_id, contest_round_id, contest_id, type, language, final_candidate, problem_final, contest_final, contest_initial_final, initial_status, full_status, submit_time, score, last_judgment, initial_report, final_report) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

	// new_conn.update("LOCK TABLES submissions WRITE, contest_problems READ");
	// auto lock_guard = make_call_in_destructor([&]{
	// 	new_conn.update("UNLOCK TABLES");
	// });

	my_bool problem_final, contest_final, contest_initial_final;
	uint initial_status, full_status;
	while (stmt.next()) {
		problem_final = false;
		contest_final = false;
		contest_initial_final = false;

		switch (old::SubmissionType(type)) {
		case old::SubmissionType::NORMAL:
			type = uint(SubmissionType::NORMAL);
			break;
		case old::SubmissionType::VOID:
			type = uint(SubmissionType::VOID);
			break;
		case old::SubmissionType::FINAL:
			type = uint(SubmissionType::NORMAL);
			contest_final = true;
			break;
		case old::SubmissionType::IGNORED:
			type = uint(SubmissionType::IGNORED);
			break;
		case old::SubmissionType::PROBLEM_SOLUTION:
			type = uint(SubmissionType::PROBLEM_SOLUTION);
			break;
		}

		old::SubmissionStatus old_status = old::SubmissionStatus(status);
		if (old_status == old::SubmissionStatus::PENDING)
			initial_status = full_status = (uint)SubmissionStatus::PENDING;
		else if (old_status == old::SubmissionStatus::COMPILATION_ERROR)
			initial_status = full_status = (uint)SubmissionStatus::COMPILATION_ERROR;
		else if (old_status == old::SubmissionStatus::CHECKER_COMPILATION_ERROR)
			initial_status = full_status = (uint)SubmissionStatus::CHECKER_COMPILATION_ERROR;
		else if (old_status == old::SubmissionStatus::JUDGE_ERROR)
			initial_status = full_status = (uint)SubmissionStatus::JUDGE_ERROR;
		else {
			// Initial
			if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_OK)
				initial_status = (uint)SubmissionStatus::OK;
			else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_WA)
				initial_status = (uint)SubmissionStatus::WA;
			else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_TLE)
				initial_status = (uint)SubmissionStatus::TLE;
			else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_MLE)
				initial_status = (uint)SubmissionStatus::MLE;
			else if ((old_status & old::SubmissionStatus::INITIAL_MASK) == old::SubmissionStatus::INITIAL_RTE)
				initial_status = (uint)SubmissionStatus::RTE;
			// Full
			if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::OK)
				full_status = (uint)SubmissionStatus::OK;
			else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::WA)
				full_status = (uint)SubmissionStatus::WA;
			else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::TLE)
				full_status = (uint)SubmissionStatus::TLE;
			else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::MLE)
				full_status = (uint)SubmissionStatus::MLE;
			else if ((old_status & old::SubmissionStatus::FINAL_MASK) == old::SubmissionStatus::RTE)
				full_status = (uint)SubmissionStatus::RTE;
		}

		inserter.bind_all(id, bind_arg(owner, isnull_owner), problem_id, bind_arg(contest_problem_id, isnull_contest_problem_id), bind_arg(contest_round_id, isnull_contest_round_id), bind_arg(contest_id, isnull_contest_id), type, language, final_candidate, problem_final, contest_final, contest_initial_final, initial_status, full_status, submit_time, bind_arg(score, isnull_score), last_judgment, initial_report, final_report);
		inserter.execute();

		submission::update_final(new_conn, owner, problem_id, contest_problem_id);
	}

	// assert(false);

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
