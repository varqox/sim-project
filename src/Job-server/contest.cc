#include "main.h"

#include <sim/constants.h>
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
