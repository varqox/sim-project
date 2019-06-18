#include "problem_reupload_job_handler.h"
#include "main.h"

void ProblemReuploadJobHandler::run() {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_read_committed_transaction();
	// Need to create the package
	if (not tmp_file_id.has_value()) {
		build_package();
		if (failed())
			return;

		if (need_model_solution_judge_report) {
			bool canceled;
			job_done(canceled);
			if (not canceled) {
				transaction.commit();
				package_file_remover.cancel();
				return;
			}
		}
	}

	replace_problem_in_DB();

	submit_solutions();

	bool canceled;
	job_done(canceled);

	if (not failed() and not canceled) {
		transaction.commit();
		package_file_remover.cancel();
		return;
	}
}
