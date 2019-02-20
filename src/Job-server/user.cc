#include "main.h"

#include <sim/constants.h>
#include <sim/submission.h>

void delete_user(uint64_t job_id, StringView user_id) {
	InplaceBuff<256> job_log;

	auto set_failure = [&](auto&&... args) {
		job_log.append(std::forward<decltype(args)>(args)...);
		mysql.prepare("UPDATE jobs"
			" SET status=" JSTATUS_FAILED_STR ", data=?"
			" WHERE id=? AND status!=" JSTATUS_CANCELED_STR)
			.bindAndExecute(job_log, job_id);

		stdlog("Job ", job_id, ":\n", job_log);
	};

	// TODO: create transaction - check user information, add jobs to delete submission files (when the global file-system will be created, and then delete the user with all consequent actions)

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

	// Delete user (all necessary actions will take place thanks to foreign key
	// constrains)
	mysql.prepare("DELETE FROM users WHERE id=?").bindAndExecute(user_id);

	mysql.prepare("UPDATE jobs"
		" SET status=" JSTATUS_DONE_STR ", data=? WHERE id=?")
		.bindAndExecute(job_log, job_id);
}
