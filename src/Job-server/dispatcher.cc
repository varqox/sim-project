#include "change_problem_statement_job_handler.h"
#include "contest_problem_reselect_final_submissions.h"
#include "delete_contest_job_handler.h"
#include "delete_contest_problem_job_handler.h"
#include "delete_contest_round_job_handler.h"
#include "delete_internal_file_job_handler.h"
#include "delete_problem_job_handler.h"
#include "delete_user_job_handler.h"
#include "judge_or_rejudge_job_handler.h"
#include "main.h"
#include "merge_problems_job_handler.h"
#include "problem_add_job_handler.h"
#include "problem_add_judge_model_solution_job_handler.h"
#include "problem_reupload_job_handler.h"
#include "problem_reupload_judge_model_solution_job_handler.h"
#include "reset_problem_time_limits_job_handler.h"

#include <thread>

void job_dispatcher(uint64_t job_id, JobType jtype, Optional<uint64_t> file_id,
                    Optional<uint64_t> tmp_file_id,
                    Optional<StringView> creator, Optional<uint64_t> aux_id,
                    StringView info, StringView added) {
	STACK_UNWINDING_MARK;
	using std::make_unique;
	std::unique_ptr<JobHandler> job_handler;
	try {
		using JT = JobType;
		switch (jtype) {
		case JT::ADD_PROBLEM:
			job_handler = make_unique<ProblemAddJobHandler>(
			   job_id, creator.value(), info, file_id.value(), tmp_file_id);
			break;

		case JT::REUPLOAD_PROBLEM:
			job_handler = make_unique<ProblemReuploadJobHandler>(
			   job_id, creator.value(), info, file_id.value(), tmp_file_id,
			   aux_id.value());
			break;

		case JT::EDIT_PROBLEM:
			std::this_thread::sleep_for(std::chrono::seconds(1));
			mysql.update(intentionalUnsafeStringView(concat(
			   "UPDATE jobs SET status=" JSTATUS_CANCELED_STR " WHERE id=",
			   job_id)));
			return;

		case JT::DELETE_PROBLEM:
			job_handler =
			   make_unique<DeleteProblemJobHandler>(job_id, aux_id.value());
			break;

		case JT::MERGE_PROBLEMS:
			job_handler = make_unique<MergeProblemsJobHandler>(
			   job_id, aux_id.value(), jobs::MergeProblemsInfo(info));
			break;

		case JT::DELETE_USER:
			job_handler =
			   make_unique<DeleteUserJobHandler>(job_id, aux_id.value());
			break;

		case JT::CONTEST_PROBLEM_RESELECT_FINAL_SUBMISSIONS:
			job_handler =
			   make_unique<ContestProblemReselectFinalSubmissionsJobHandler>(
			      job_id, aux_id.value());
			break;

		case JT::DELETE_CONTEST:
			job_handler =
			   make_unique<DeleteContestJobHandler>(job_id, aux_id.value());
			break;

		case JT::DELETE_CONTEST_ROUND:
			job_handler = make_unique<DeleteContestRoundJobHandler>(
			   job_id, aux_id.value());
			break;

		case JT::DELETE_CONTEST_PROBLEM:
			job_handler = make_unique<DeleteContestProblemJobHandler>(
			   job_id, aux_id.value());
			break;

		case JT::DELETE_FILE:
			job_handler = make_unique<DeleteInternalFileJobHandler>(
			   job_id, file_id.value());
			break;

		case JT::CHANGE_PROBLEM_STATEMENT:
			job_handler = make_unique<ChangeProblemStatementJobHandler>(
			   job_id, aux_id.value(), file_id.value(),
			   jobs::ChangeProblemStatementInfo(info));
			break;

		case JT::JUDGE_SUBMISSION:
		case JT::REJUDGE_SUBMISSION:
			job_handler = make_unique<JudgeOrRejudgeJobHandler>(
			   job_id, aux_id.value(), added);
			break;

		case JT::ADD_JUDGE_MODEL_SOLUTION:
			job_handler = make_unique<ProblemAddJudgeModelSolutionJobHandler>(
			   job_id, creator.value(), info, file_id.value(), tmp_file_id);
			break;

		case JT::REUPLOAD_JUDGE_MODEL_SOLUTION:
			job_handler =
			   make_unique<ProblemReuploadJudgeModelSolutionJobHandler>(
			      job_id, creator.value(), info, file_id.value(), tmp_file_id,
			      aux_id.value());
			break;

		case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
			job_handler = make_unique<ResetProblemTimeLimitsJobHandler>(
			   job_id, aux_id.value());
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
