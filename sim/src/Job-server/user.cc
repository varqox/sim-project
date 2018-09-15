#include "main.h"

#include <sim/constants.h>
#include <sim/submission.h>

void delete_user(uint64_t job_id, StringView user_id) {
	// Run deletion twice - because race condition may occur and e.g. user may
	// add submission after the submissions were deleted (thus it would not be
	// deleted). Other order e.g. removing contest first would expose corrupted
	// structure during the whole process of deletion.
	for (int i = 0; i < 2; ++i) {
		// Delete submissions
		{
			auto stmt = mysql.prepare("SELECT id FROM submissions"
				" WHERE owner=?");
			stmt.bindAndExecute(user_id);
			InplaceBuff<20> submission_id;
			stmt.res_bind_all(submission_id);
			while (stmt.next())
				submission::delete_submission(mysql, submission_id);
		}

		// Change files creator to NULL
		auto stmt = mysql.prepare("UPDATE files SET creator=NULL"
			" WHERE creator=?");
		stmt.bindAndExecute(user_id);

		// // Change problem owner to SIM ROOT
		// stmt = mysql.prepare("UPDATE problems SET owner=" SIM_ROOT_UID
		// 	" WHERE creator=?");
		// stmt.bindAndExecute(user_id);

		// Delete form contest users
		stmt = mysql.prepare("DELETE FROM contest_users WHERE user_id=?");
		stmt.bindAndExecute(user_id);

		// Delete sessions
		stmt = mysql.prepare("DELETE FROM session WHERE user_id=?");
		stmt.bindAndExecute(user_id);

		// Delete user
		stmt = mysql.prepare("DELETE FROM users WHERE id=?");
		stmt.bindAndExecute(user_id);
	}

	auto stmt = mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data='' WHERE id=?");
	stmt.bindAndExecute(job_id);
}
