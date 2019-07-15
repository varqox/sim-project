#include "job_handlers/add_problem.h"
#include "job_handlers/add_problem__judge_model_solution.h"
#include "job_handlers/change_problem_statement.h"
#include "job_handlers/delete_contest.h"
#include "job_handlers/delete_contest_problem.h"
#include "job_handlers/delete_contest_round.h"
#include "job_handlers/delete_internal_file.h"
#include "job_handlers/delete_problem.h"
#include "job_handlers/delete_user.h"
#include "job_handlers/job_handler.h"
#include "job_handlers/judge_or_rejudge.h"
#include "job_handlers/merge_problems.h"
#include "job_handlers/reselect_final_submissions_in_contest_problem.h"
#include "job_handlers/reset_problem_time_limits.h"
#include "job_handlers/reupload_problem.h"
#include "job_handlers/reupload_problem__judge_model_solution.h"
#include "main.h"

#include <thread>

void job_dispatcher(uint64_t job_id, JobType jtype,
                    std::optional<uint64_t> file_id,
                    std::optional<uint64_t> tmp_file_id,
                    std::optional<StringView> creator,
                    std::optional<uint64_t> aux_id, StringView info,
                    StringView added) {
	STACK_UNWINDING_MARK;
	using std::make_unique;
	std::unique_ptr<job_handlers::JobHandler> job_handler;
	try {
		using namespace job_handlers;
		using JT = JobType;

		switch (jtype) {
		case JT::ADD_PROBLEM:
			job_handler = make_unique<AddProblem>(job_id, creator.value(), info,
			                                      file_id.value(), tmp_file_id);
			break;

		case JT::REUPLOAD_PROBLEM:
			job_handler = make_unique<ReuploadProblem>(
			   job_id, creator.value(), info, file_id.value(), tmp_file_id,
			   aux_id.value());
			break;

		case JT::EDIT_PROBLEM:
			// TODO: implement it (editing Simfile for now)
			std::this_thread::sleep_for(std::chrono::seconds(1));
			mysql.update(intentionalUnsafeStringView(concat(
			   "UPDATE jobs SET status=" JSTATUS_CANCELED_STR " WHERE id=",
			   job_id)));
			return;

		case JT::DELETE_PROBLEM:
			job_handler = make_unique<DeleteProblem>(job_id, aux_id.value());
			break;

		case JT::MERGE_PROBLEMS:
			job_handler = make_unique<MergeProblems>(
			   job_id, aux_id.value(), jobs::MergeProblemsInfo(info));
			break;

		case JT::DELETE_USER:
			job_handler = make_unique<DeleteUser>(job_id, aux_id.value());
			break;

		case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
			job_handler = make_unique<ReselectFinalSubmissionsInContestProblem>(
			   job_id, aux_id.value());
			break;

		case JT::DELETE_CONTEST:
			job_handler = make_unique<DeleteContest>(job_id, aux_id.value());
			break;

		case JT::DELETE_CONTEST_ROUND:
			job_handler =
			   make_unique<DeleteContestRound>(job_id, aux_id.value());
			break;

		case JT::DELETE_CONTEST_PROBLEM:
			job_handler =
			   make_unique<DeleteContestProblem>(job_id, aux_id.value());
			break;

		case JT::DELETE_FILE:
			job_handler =
			   make_unique<DeleteInternalFile>(job_id, file_id.value());
			break;

		case JT::CHANGE_PROBLEM_STATEMENT:
			job_handler = make_unique<ChangeProblemStatement>(
			   job_id, aux_id.value(), file_id.value(),
			   jobs::ChangeProblemStatementInfo(info));
			break;

		case JT::JUDGE_SUBMISSION:
		case JT::REJUDGE_SUBMISSION:
			job_handler =
			   make_unique<JudgeOrRejudge>(job_id, aux_id.value(), added);
			break;

		case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
			job_handler = make_unique<AddProblemJudgeModelSolution>(
			   job_id, creator.value(), info, file_id.value(), tmp_file_id);
			break;

		case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
			job_handler = make_unique<ReuploadProblemJudgeModelSolution>(
			   job_id, creator.value(), info, file_id.value(), tmp_file_id,
			   aux_id.value());
			break;

		case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
			job_handler =
			   make_unique<ResetProblemTimeLimits>(job_id, aux_id.value());
			break;
		}

		throw_assert(job_handler);
		job_handler->run();
		if (job_handler->failed()) {
			mysql
			   .prepare("UPDATE jobs"
			            " SET status=" JSTATUS_FAILED_STR ", data=? WHERE id=?")
			   .bindAndExecute(job_handler->get_log(), job_id);
		}

	} catch (const std::exception& e) {
		ERRLOG_CATCH(e);
		auto transaction = mysql.start_transaction();

		// Add job to delete temporary file
		auto stmt = mysql.prepare(
		   "INSERT INTO jobs(file_id, creator, type,"
		   " priority, status, added, aux_id, info, data) "
		   "SELECT tmp_file_id, NULL, " JTYPE_DELETE_FILE_STR
		   ", ?, " JSTATUS_PENDING_STR ", ?, NULL, '', '' FROM jobs"
		   " WHERE id=? AND tmp_file_id IS NOT NULL");
		stmt.bindAndExecute(priority(JobType::DELETE_FILE), mysql_date(),
		                    job_id);

		// Fail job
		stmt = mysql.prepare("UPDATE jobs "
		                     "SET tmp_file_id=NULL, status=" JSTATUS_FAILED_STR
		                     ", data=? WHERE id=?");
		if (job_handler) {
			stmt.bindAndExecute(
			   concat(job_handler->get_log(), "\nCaught exception: ", e.what()),
			   job_id);
		} else {
			stmt.bindAndExecute(concat("\nCaught exception: ", e.what()),
			                    job_id);
		}

		transaction.commit();
	}
}
