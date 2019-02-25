#include "delete_contest_job_handler.h"
#include "main.h"

#include <sim/constants.h>

void DeleteContestJobHandler::run() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	// Log some info about the deleted contest
	{
		auto stmt = mysql.prepare("SELECT name FROM contests WHERE id=?");
		stmt.bindAndExecute(contest_id);
		InplaceBuff<32> cname;
		stmt.res_bind_all(cname);
		if (not stmt.next()) {
			return set_failure("Contest with id: ", contest_id,
				" does not exist");
		}

		job_log("Contest: ", cname, " (", contest_id, ")");
	}

	// Add jobs to delete submission files
	mysql.prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
			" added, aux_id, info, data)"
		" SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR ", ?, "
			JSTATUS_PENDING_STR ", ?, NULL, '', ''"
		" FROM submissions WHERE contest_id=?")
		.bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(), contest_id);

	// Add jobs to delete contest files
	mysql.prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
			" added, aux_id, info, data)"
		" SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR ", ?, "
			JSTATUS_PENDING_STR ", ?, NULL, '', ''"
		" FROM files WHERE contest_id=?")
		.bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(), contest_id);

	// Delete contest (all necessary actions will take place thanks to foreign
	// key constrains)
	mysql.prepare("DELETE FROM contests WHERE id=?").bindAndExecute(contest_id);

	job_done();

	transaction.commit();
}
