#include "merge_problems.hh"
#include "../main.hh"

#include <deque>
#include <sim/constants.hh>
#include <sim/submission.hh>

namespace job_handlers {

void MergeProblems::run() {
	STACK_UNWINDING_MARK;

	for (;;) {
		try {
			run_impl();
			break;
		} catch (const std::exception& e) {
			if (has_prefix(e.what(), "Deadlock found when trying to get lock; "
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
		stmt.bind_and_execute(donor_problem_id_);
		InplaceBuff<1> simfile;
		stmt.res_bind_all(simfile);
		if (not stmt.next())
			return set_failure("Problem does not exist");

		job_log("Merged problem Simfile:\n", simfile);

		stmt = mysql.prepare("SELECT 1 FROM problems WHERE id=?");
		stmt.bind_and_execute(info_.target_problem_id);
		if (not stmt.next())
			return set_failure("Target problem does not exist");
	}

	// Transfer contest problems
	mysql.prepare("UPDATE contest_problems SET problem_id=? WHERE problem_id=?")
	   .bind_and_execute(info_.target_problem_id, donor_problem_id_);

	// Add job to delete problem file
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data) "
	            "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', '' "
	            "FROM problems WHERE id=?")
	   .bind_and_execute(
	      EnumVal(JobType::DELETE_FILE), priority(JobType::DELETE_FILE),
	      EnumVal(JobStatus::PENDING), mysql_date(), donor_problem_id_);

	// Add jobs to delete problem solutions' files
	mysql
	   .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
	            " added, aux_id, info, data) "
	            "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', '' "
	            "FROM submissions WHERE problem_id=? AND "
	            "type=?")
	   .bind_and_execute(
	      EnumVal(JobType::DELETE_FILE), priority(JobType::DELETE_FILE),
	      EnumVal(JobStatus::PENDING), mysql_date(), donor_problem_id_,
	      EnumVal(SubmissionType::PROBLEM_SOLUTION));

	// Delete problem solutions
	mysql
	   .prepare("DELETE FROM submissions "
	            "WHERE problem_id=? AND type=?")
	   .bind_and_execute(donor_problem_id_,
	                     EnumVal(SubmissionType::PROBLEM_SOLUTION));

	// Collect update finals
	struct FTU {
		MySQL::Optional<uint64_t> owner;
		MySQL::Optional<uint64_t> contest_problem_id;
	};
	std::deque<FTU> finals_to_update;
	{
		auto stmt = mysql.prepare("SELECT DISTINCT owner, contest_problem_id "
		                          "FROM submissions WHERE problem_id=?");
		stmt.bind_and_execute(donor_problem_id_);
		FTU ftu_elem;
		stmt.res_bind_all(ftu_elem.owner, ftu_elem.contest_problem_id);
		while (stmt.next())
			finals_to_update.emplace_back(ftu_elem);
	}

	// Schedule rejudge of the transferred submissions
	if (info_.rejudge_transferred_submissions) {
		mysql
		   .prepare("INSERT INTO jobs(creator, status, priority, type, added,"
		            " aux_id, info, data) "
		            "SELECT NULL, ?, ?, ?, ?, id, ?, '' "
		            "FROM submissions WHERE problem_id=? ORDER BY id")
		   .bind_and_execute(EnumVal(JobStatus::PENDING),
		                     priority(JobType::REJUDGE_SUBMISSION),
		                     EnumVal(JobType::REJUDGE_SUBMISSION), mysql_date(),
		                     jobs::dump_string(intentional_unsafe_string_view(
		                        to_string(info_.target_problem_id))),
		                     donor_problem_id_);
	}

	// Transfer problem submissions that are not problem solutions
	mysql.prepare("UPDATE submissions SET problem_id=? WHERE problem_id=?")
	   .bind_and_execute(info_.target_problem_id, donor_problem_id_);

	// Update finals (both contest and problem finals are being taken care of)
	for (auto const& ftu_elem : finals_to_update) {
		submission::update_final_lock(mysql, ftu_elem.owner,
		                              info_.target_problem_id);
		submission::update_final(mysql, ftu_elem.owner, info_.target_problem_id,
		                         ftu_elem.contest_problem_id, false);
	}

	// Transfer problem tags (duplicates will not be transferred - they will be
	// deleted on problem deletion)
	mysql
	   .prepare(
	      "UPDATE IGNORE problem_tags SET problem_id=? WHERE problem_id=?")
	   .bind_and_execute(info_.target_problem_id, donor_problem_id_);

	// Finally, delete the donor problem (solution and remaining tags will be
	// deleted automatically thanks to foreign key constraints)
	mysql.prepare("DELETE FROM problems WHERE id=?")
	   .bind_and_execute(donor_problem_id_);

	job_done();
	transaction.commit();
}

} // namespace job_handlers
