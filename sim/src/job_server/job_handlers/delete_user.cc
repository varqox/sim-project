#include "delete_user.h"

#include "../main.h"

#include <sim/constants.h>
#include <sim/user.hh>

using sim::User;

namespace job_handlers {

void DeleteUser::run() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	// Log some info about the deleted user
	{
		auto stmt =
		   mysql.prepare("SELECT username, type FROM users WHERE id=?");
		stmt.bindAndExecute(user_id_);
		InplaceBuff<32> username;
		decltype(User::type) user_type;
		stmt.res_bind_all(username, user_type);
		if (not stmt.next())
			return set_failure("User with id: ", user_id_, " does not exist");

		job_log("username: ", username);
		switch (user_type) {
		case User::Type::ADMIN: job_log("type: admin"); break;
		case User::Type::TEACHER: job_log("type: teacher"); break;
		case User::Type::NORMAL: job_log("type: normal"); break;
		}
	}

	// Add jobs to delete submission files
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data) "
	            "SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR
	            ", ?, " JSTATUS_PENDING_STR ", ?, NULL, '', ''"
	            " FROM submissions WHERE owner=?")
	   .bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(), user_id_);

	// Delete user (all necessary actions will take place thanks to foreign key
	// constrains)
	mysql.prepare("DELETE FROM users WHERE id=?").bindAndExecute(user_id_);

	job_done();

	transaction.commit();
}

} // namespace job_handlers
