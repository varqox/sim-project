#include "main.h"

#include <sim/submission.h>

void contest_problem_reselect_final_submissions(uint64_t job_id,
	StringView aux_id)
{
	// Lock the table to be able to safely modify the submission
	// locking contest_problems is required by update_final()
	mysql.update("LOCK TABLES submissions WRITE, contest_problems READ");
	auto lock_guard = make_call_in_destructor([&]{
		mysql.update("UNLOCK TABLES");
	});

	auto stmt = mysql.prepare("SELECT owner FROM submissions"
		" WHERE contest_problem_id=? AND type=" STYPE_FINAL_STR);
	stmt.bindAndExecute(aux_id);

	InplaceBuff<30> sowner;
	stmt.res_bind_all(sowner);
	while (stmt.next())
		submission::update_final(mysql, sowner, aux_id, false);

	// Done
	lock_guard.call_and_cancel();
	stmt = mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data='' WHERE id=?");
	stmt.bindAndExecute(job_id);
}
