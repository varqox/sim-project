#include "main.h"

#include <sim/constants.h>
#include <sim/file.h>
#include <sim/submission.h>

void contest_problem_reselect_final_submissions(uint64_t job_id,
	StringView contest_problem_id)
{
	// Lock the table to be able to safely modify the submission
	// locking contest_problems is required by update_final()
	mysql.update("LOCK TABLES submissions WRITE, contest_problems READ");
	auto lock_guard = make_call_in_destructor([&]{
		mysql.update("UNLOCK TABLES");
	});

	auto stmt = mysql.prepare("SELECT owner, problem_id FROM submissions"
		" WHERE contest_problem_id=? AND contest_final=1");
	stmt.bindAndExecute(contest_problem_id);

	InplaceBuff<30> sowner;
	InplaceBuff<30> problem_id;
	stmt.res_bind_all(sowner, problem_id);
	while (stmt.next())
		submission::update_final(mysql, sowner, problem_id, contest_problem_id, false);

	// Done
	lock_guard.call_and_cancel();
	stmt = mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data='' WHERE id=?");
	stmt.bindAndExecute(job_id);
}

void delete_contest(uint64_t job_id, StringView contest_id) {
	// Run deletion twice - because race condition may occur and e.g. user may
	// add submission after the submissions were deleted (thus it would not be
	// deleted). Other order e.g. removing contest first would expose corrupted
	// structure during the whole process of deletion.
	for (int i = 0; i < 2; ++i) {
		// Delete submissions
		{
			auto stmt = mysql.prepare("SELECT id FROM submissions"
				" WHERE contest_id=?");
			stmt.bindAndExecute(contest_id);
			InplaceBuff<20> submission_id;
			stmt.res_bind_all(submission_id);
			while (stmt.next())
				submission::delete_submission(mysql, submission_id);
		}

		// Delete files
		{
			auto stmt = mysql.prepare("SELECT id FROM files"
				" WHERE contest_id=?");
			stmt.bindAndExecute(contest_id);
			InplaceBuff<FILE_ID_LEN> file_id;
			stmt.res_bind_all(file_id);
			while (stmt.next())
				file::delete_file(mysql, file_id);
		}

		// Delete contest users
		auto stmt = mysql.prepare("DELETE FROM contest_users WHERE contest_id=?");
		stmt.bindAndExecute(contest_id);

		// Delete contest entry tokens
		stmt = mysql.prepare("DELETE FROM contest_entry_tokens"
			" WHERE contest_id=?");
		stmt.bindAndExecute(contest_id);

		// Delete contest problems
		stmt = mysql.prepare("DELETE FROM contest_problems WHERE contest_id=?");
		stmt.bindAndExecute(contest_id);

		// Delete contest rounds
		stmt = mysql.prepare("DELETE FROM contest_rounds WHERE contest_id=?");
		stmt.bindAndExecute(contest_id);

		// Delete contest
		stmt = mysql.prepare("DELETE FROM contests WHERE id=?");
		stmt.bindAndExecute(contest_id);
	}

	auto stmt = mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data='' WHERE id=?");
	stmt.bindAndExecute(job_id);
}

void delete_contest_round(uint64_t job_id, StringView contest_round_id) {
	// Run deletion twice - because race condition may occur and e.g. user may
	// add submission after the submissions were deleted (thus it would not be
	// deleted). Other order e.g. removing contest round first would expose
	// corrupted structure during the whole process of deletion.
	for (int i = 0; i < 2; ++i) {
		// Delete submissions
		{
			auto stmt = mysql.prepare("SELECT id FROM submissions"
				" WHERE contest_round_id=?");
			stmt.bindAndExecute(contest_round_id);
			InplaceBuff<20> submission_id;
			stmt.res_bind_all(submission_id);
			while (stmt.next())
				submission::delete_submission(mysql, submission_id);
		}

		// Delete contest problems
		auto stmt = mysql.prepare("DELETE FROM contest_problems WHERE contest_round_id=?");
		stmt.bindAndExecute(contest_round_id);

		// Delete contest round
		stmt = mysql.prepare("DELETE FROM contest_rounds WHERE id=?");
		stmt.bindAndExecute(contest_round_id);
	}

	auto stmt = mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data='' WHERE id=?");
	stmt.bindAndExecute(job_id);
}

void delete_contest_problem(uint64_t job_id, StringView contest_problem_id) {
	// Run deletion twice - because race condition may occur and e.g. user may
	// add submission after the submissions were deleted (thus it would not be
	// deleted). Other order e.g. removing contest problem first would expose
	// corrupted structure during the whole process of deletion.
	for (int i = 0; i < 2; ++i) {
		// Delete submissions
		{
			auto stmt = mysql.prepare("SELECT id FROM submissions"
				" WHERE contest_problem_id=?");
			stmt.bindAndExecute(contest_problem_id);
			InplaceBuff<20> submission_id;
			stmt.res_bind_all(submission_id);
			while (stmt.next())
				submission::delete_submission(mysql, submission_id);
		}

		// Delete contest problem
		auto stmt = mysql.prepare("DELETE FROM contest_problems WHERE id=?");
		stmt.bindAndExecute(contest_problem_id);
	}

	auto stmt = mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data='' WHERE id=?");
	stmt.bindAndExecute(job_id);
}
