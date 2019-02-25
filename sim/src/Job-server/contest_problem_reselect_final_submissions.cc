#include "contest_problem_reselect_final_submissions.h"
#include "main.h"

#include <sim/submission.h>

void ContestProblemReselectFinalSubmissionsJobHandler::run() {
	STACK_UNWINDING_MARK;

	// Serializable transaction is required by update_final
	auto transaction = mysql.start_serializable_transaction();

	auto stmt = mysql.prepare("SELECT DISTINCT owner, problem_id"
		" FROM submissions WHERE contest_problem_id=? AND final_candidate=1");
	stmt.bindAndExecute(contest_problem_id);

	MySQL::Optional<uint64_t> sowner;
	uint64_t problem_id;
	stmt.res_bind_all(sowner, problem_id);
	while (stmt.next())
		submission::update_final(mysql, sowner, problem_id, contest_problem_id, false);

	job_done();

	transaction.commit();
}
