#include "contest_problem_reselect_final_submissions.h"
#include "main.h"

#include <sim/submission.h>

void ContestProblemReselectFinalSubmissionsJobHandler::run() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	auto stmt = mysql.prepare("SELECT DISTINCT owner, problem_id"
		" FROM submissions WHERE contest_problem_id=? AND final_candidate=1"
		" ORDER by id"); // Order is important so that if the two such jobs are
	                     // running, one would block until the other finishes
	stmt.bindAndExecute(contest_problem_id);

	MySQL::Optional<uint64_t> sowner;
	uint64_t problem_id;
	stmt.res_bind_all(sowner, problem_id);
	while (stmt.next()) {
		submission::update_final_lock(mysql, sowner, problem_id);
		submission::update_final(mysql, sowner, problem_id, contest_problem_id,
			false);
	}

	job_done();

	transaction.commit();
}
