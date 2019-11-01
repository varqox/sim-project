#include "delete_contest.h"
#include "../main.h"

#include <sim/constants.h>

namespace job_handlers {

void DeleteContest::run() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	// Log some info about the deleted contest
	{
		auto stmt = mysql.prepare("SELECT name FROM contests WHERE id=?");
		stmt.bindAndExecute(contest_id_);
		InplaceBuff<32> cname;
		stmt.res_bind_all(cname);
		if (not stmt.next()) {
			return set_failure("Contest with id: ", contest_id_,
			                   " does not exist");
		}

		job_log("Contest: ", cname, " (", contest_id_, ")");
	}

	// Add jobs to delete submission files
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data)"
	            " SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR
	            ", ?, " JSTATUS_PENDING_STR ", ?, NULL, '', ''"
	            " FROM submissions WHERE contest_id=?")
	   .bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(),
	                   contest_id_);

	// Add jobs to delete contest files
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data)"
	            " SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR
	            ", ?, " JSTATUS_PENDING_STR ", ?, NULL, '', ''"
	            " FROM contest_files WHERE contest_id=?")
	   .bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(),
	                   contest_id_);

	// Delete contest (all necessary actions will take place thanks to foreign
	// key constrains)
	mysql.prepare("DELETE FROM contests WHERE id=?")
	   .bindAndExecute(contest_id_);

	job_done();

	transaction.commit();
}

} // namespace job_handlers
