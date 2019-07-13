#include "delete_contest_round.h"
#include "../main.h"

#include <sim/constants.h>

namespace job_handlers {

void DeleteContestRound::run() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	// Log some info about the deleted contest round
	{
		auto stmt = mysql.prepare("SELECT c.name, c.id, r.name"
		                          " FROM contest_rounds r"
		                          " JOIN contests c ON c.id=r.contest_id"
		                          " WHERE r.id=?");
		stmt.bindAndExecute(contest_round_id_);
		InplaceBuff<32> cname, cid, rname;
		stmt.res_bind_all(cname, cid, rname);
		if (not stmt.next()) {
			return set_failure("Contest round with id: ", contest_round_id_,
			                   " does not exist or the contest hierarchy is "
			                   "broken (likely the former).");
		}

		job_log("Contest: ", cname, " (", cid, ')');
		job_log("Contest round: ", rname, " (", contest_round_id_, ')');
	}

	// Add jobs to delete submission files
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data) "
	            "SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR
	            ", ?, " JSTATUS_PENDING_STR ", ?, NULL, '', ''"
	            " FROM submissions WHERE contest_round_id=?")
	   .bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(),
	                   contest_round_id_);

	// Delete contest round (all necessary actions will take place thanks to
	// foreign key constrains)
	mysql.prepare("DELETE FROM contest_rounds WHERE id=?")
	   .bindAndExecute(contest_round_id_);

	job_done();

	transaction.commit();
}

} // namespace job_handlers
