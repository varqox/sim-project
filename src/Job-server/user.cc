#include "main.h"

#include <sim/constants.h>
#include <sim/submission.h>

void delete_user(uint64_t job_id, StringView user_id) {
	InplaceBuff<256> job_log;

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		auto stmt = mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR);
		stmt.bindAndExecute(job_log, job_id);

		stdlog("Job ", job_id, ":\n", job_log);
	};

	// Log some info about the deleted user
	{
		auto stmt = mysql.prepare("SELECT username, type FROM users WHERE id=?");
		stmt.bindAndExecute(user_id);
		InplaceBuff<32> username;
		EnumVal<UserType> user_type;
		stmt.res_bind_all(username, user_type);
		if (not stmt.next())
			return set_failure("User with id: ", user_id, " does not exist");

		job_log.append("username: ", username, "\n");
		switch (user_type) {
		case UserType::ADMIN: job_log.append("type: admin\n");
		case UserType::TEACHER: job_log.append("type: teacher\n");
		case UserType::NORMAL: job_log.append("type: normal\n");
		}
	}

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
		" SET status=" JSTATUS_DONE_STR ", data=? WHERE id=?");
	stmt.bindAndExecute(job_log, job_id);
}
