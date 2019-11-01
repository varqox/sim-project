#include "merge_problems.h"
#include "../main.h"

#include <deque>
#include <sim/submission.h>

namespace job_handlers {

void MergeProblems::run() {
	STACK_UNWINDING_MARK;

	for (;;) {
		try {
			run_impl();
			break;
		} catch (const std::exception& e) {
			if (hasPrefix(e.what(), "Deadlock found when trying to get lock; "
			                        "try restarting transaction")) {
				continue;
			}

			throw;
		}
	}
}

void MergeProblems::run_impl() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	// Assure that both problems exist
	{
		auto stmt = mysql.prepare("SELECT simfile FROM problems WHERE id=?");
		stmt.bindAndExecute(problem_id_);
		InplaceBuff<1> simfile;
		stmt.res_bind_all(simfile);
		if (not stmt.next())
			return set_failure("Problem does not exist");

		job_log("Merged problem Simfile:\n", simfile);

		stmt = mysql.prepare("SELECT 1 FROM problems WHERE id=?");
		stmt.bindAndExecute(info_.target_problem_id);
		if (not stmt.next())
			return set_failure("Target problem does not exist");
	}

	// Transfer contest problems
	mysql.prepare("UPDATE contest_problems SET problem_id=? WHERE problem_id=?")
	   .bindAndExecute(info_.target_problem_id, problem_id_);

	// Add job to delete problem file
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data) "
	            "SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR
	            ", ?, " JSTATUS_PENDING_STR ", ?, NULL, '', '' "
	            "FROM problems WHERE id=?")
	   .bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(),
	                   problem_id_);

	// Add jobs to delete problem solutions' files
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data) "
	            "SELECT file_id, NULL, " JTYPE_DELETE_FILE_STR
	            ", ?, " JSTATUS_PENDING_STR ", ?, NULL, '', '' "
	            "FROM submissions WHERE problem_id=? AND "
	            "type=" STYPE_PROBLEM_SOLUTION_STR)
	   .bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(),
	                   problem_id_);

	// Delete problem solutions
	mysql
	   .prepare("DELETE FROM submissions "
	            "WHERE problem_id=? AND type=" STYPE_PROBLEM_SOLUTION_STR)
	   .bindAndExecute(problem_id_);

	// Collect update finals
	std::deque<std::array<MySQL::Optional<uint64_t>, 2>> finals_to_update;
	{
		auto stmt = mysql.prepare("SELECT DISTINCT owner, contest_problem_id "
		                          "FROM submissions WHERE problem_id=?");
		stmt.bindAndExecute(problem_id_);
		std::array<MySQL::Optional<uint64_t>, 2> ftu_elem {};
		stmt.res_bind_all(ftu_elem[0], ftu_elem[1]);
		while (stmt.next())
			finals_to_update.emplace_back(ftu_elem);
	}

	// Schedule rejudge of the transferred submissions
	if (info_.rejudge_transferred_submissions) {
		mysql
		   .prepare("INSERT INTO jobs(creator, status, priority, type, added,"
		            " aux_id, info, data) "
		            "SELECT NULL, " JSTATUS_PENDING_STR
		            ", ?, " JTYPE_REJUDGE_SUBMISSION_STR ", ?, id, ?, '' "
		            "FROM submissions WHERE problem_id=? ORDER BY id")
		   .bindAndExecute(priority(JobType::REJUDGE_SUBMISSION), mysql_date(),
		                   jobs::dumpString(intentionalUnsafeStringView(
		                      toStr(info_.target_problem_id))),
		                   problem_id_);
	}

	// Transfer problem submissions that are not problem solutions
	mysql.prepare("UPDATE submissions SET problem_id=? WHERE problem_id=?")
	   .bindAndExecute(info_.target_problem_id, problem_id_);

	// Update finals
	for (auto const& ftu_elem : finals_to_update) {
		submission::update_final_lock(mysql, ftu_elem[0],
		                              info_.target_problem_id);
		submission::update_final(mysql, ftu_elem[0], info_.target_problem_id,
		                         ftu_elem[1], false);
	}

	// Transfer problem tags (duplicates will not be transferred - they will be
	// deleted on problem deletion)
	mysql
	   .prepare(
	      "UPDATE IGNORE problem_tags SET problem_id=? WHERE problem_id=?")
	   .bindAndExecute(info_.target_problem_id, problem_id_);

	// Finally, delete the problem (solution and remaining tags will be deleted
	// automatically thanks to foreign key constraints)
	mysql.prepare("DELETE FROM problems WHERE id=?")
	   .bindAndExecute(problem_id_);

	job_done();

	transaction.commit();
}

} // namespace job_handlers
