#include "job_handlers/add_problem.hh"
#include "job_handlers/add_problem__judge_main_solution.hh"
#include "job_handlers/change_problem_statement.hh"
#include "job_handlers/delete_contest.hh"
#include "job_handlers/delete_contest_problem.hh"
#include "job_handlers/delete_contest_round.hh"
#include "job_handlers/delete_internal_file.hh"
#include "job_handlers/delete_problem.hh"
#include "job_handlers/delete_user.hh"
#include "job_handlers/job_handler.hh"
#include "job_handlers/judge_or_rejudge.hh"
#include "job_handlers/merge_problems.hh"
#include "job_handlers/merge_users.hh"
#include "job_handlers/reselect_final_submissions_in_contest_problem.hh"
#include "job_handlers/reset_problem_time_limits.hh"
#include "job_handlers/reupload_problem.hh"
#include "job_handlers/reupload_problem__judge_main_solution.hh"
#include "main.hh"

#include <sim/jobs/job.hh>
#include <thread>

using sim::jobs::Job;

namespace job_server {

void job_dispatcher(
    uint64_t job_id,
    Job::Type jtype,
    std::optional<uint64_t> file_id,
    std::optional<uint64_t> tmp_file_id,
    std::optional<StringView> creator,
    std::optional<uint64_t> aux_id,
    StringView info,
    StringView added
) {
    STACK_UNWINDING_MARK;
    using std::make_unique;
    std::unique_ptr<job_handlers::JobHandler> job_handler;
    try {
        using namespace job_handlers; // NOLINT(google-build-using-namespace)
        using JT = Job::Type;

        switch (jtype) {
        case JT::ADD_PROBLEM:
            job_handler = make_unique<AddProblem>(
                job_id, creator.value(), info, file_id.value(), tmp_file_id
            );
            break;

        case JT::REUPLOAD_PROBLEM:
            job_handler = make_unique<ReuploadProblem>(
                job_id, creator.value(), info, file_id.value(), tmp_file_id, aux_id.value()
            );
            break;

        case JT::EDIT_PROBLEM:
            // TODO: implement it (editing Simfile for now)
            std::this_thread::sleep_for(std::chrono::seconds(1));
            mysql.prepare("UPDATE jobs SET status=? WHERE id=?")
                .bind_and_execute(EnumVal(Job::Status::CANCELED), job_id);
            return;

        case JT::DELETE_PROBLEM:
            job_handler = make_unique<DeleteProblem>(job_id, aux_id.value());
            break;

        case JT::MERGE_PROBLEMS:
            job_handler = make_unique<MergeProblems>(
                job_id, aux_id.value(), sim::jobs::MergeProblemsInfo(info)
            );
            break;

        case JT::DELETE_USER: job_handler = make_unique<DeleteUser>(job_id, aux_id.value()); break;

        case JT::MERGE_USERS:
            job_handler =
                make_unique<MergeUsers>(job_id, aux_id.value(), sim::jobs::MergeUsersInfo(info));
            break;

        case JT::RESELECT_FINAL_SUBMISSIONS_IN_CONTEST_PROBLEM:
            job_handler =
                make_unique<ReselectFinalSubmissionsInContestProblem>(job_id, aux_id.value());
            break;

        case JT::DELETE_CONTEST:
            job_handler = make_unique<DeleteContest>(job_id, aux_id.value());
            break;

        case JT::DELETE_CONTEST_ROUND:
            job_handler = make_unique<DeleteContestRound>(job_id, aux_id.value());
            break;

        case JT::DELETE_CONTEST_PROBLEM:
            job_handler = make_unique<DeleteContestProblem>(job_id, aux_id.value());
            break;

        case JT::DELETE_FILE:
            job_handler = make_unique<DeleteInternalFile>(job_id, file_id.value());
            break;

        case JT::CHANGE_PROBLEM_STATEMENT:
            job_handler = make_unique<ChangeProblemStatement>(
                job_id, aux_id.value(), file_id.value(), sim::jobs::ChangeProblemStatementInfo(info)
            );
            break;

        case JT::JUDGE_SUBMISSION:
        case JT::REJUDGE_SUBMISSION:
            job_handler = make_unique<JudgeOrRejudge>(job_id, aux_id.value(), added);
            break;

        case JT::ADD_PROBLEM__JUDGE_MODEL_SOLUTION:
            job_handler = make_unique<AddProblemJudgeModelSolution>(
                job_id, creator.value(), info, file_id.value(), tmp_file_id
            );
            break;

        case JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION:
            job_handler = make_unique<ReuploadProblemJudgeModelSolution>(
                job_id, creator.value(), info, file_id.value(), tmp_file_id, aux_id.value()
            );
            break;

        case JT::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION:
            job_handler = make_unique<ResetProblemTimeLimits>(job_id, aux_id.value());
            break;
        }

        throw_assert(job_handler);
        job_handler->run();
        if (job_handler->failed()) {
            mysql.prepare("UPDATE jobs SET status=?, data=? WHERE id=?")
                .bind_and_execute(EnumVal(Job::Status::FAILED), job_handler->get_log(), job_id);
        }

    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        auto transaction = mysql.start_transaction();

        // Add job to delete temporary file
        auto stmt = mysql.prepare("INSERT INTO jobs(file_id, creator, type,"
                                  " priority, status, added, aux_id, info, data) "
                                  "SELECT tmp_file_id, NULL, ?, ?, ?, ?, NULL, '', '' FROM jobs"
                                  " WHERE id=? AND tmp_file_id IS NOT NULL");
        stmt.bind_and_execute(
            EnumVal(Job::Type::DELETE_FILE),
            default_priority(Job::Type::DELETE_FILE),
            EnumVal(Job::Status::PENDING),
            mysql_date(),
            job_id
        );

        // Fail job
        stmt = mysql.prepare("UPDATE jobs SET tmp_file_id=NULL, status=?, data=? "
                             "WHERE id=?");
        if (job_handler) {
            stmt.bind_and_execute(
                EnumVal(Job::Status::FAILED),
                concat(job_handler->get_log(), "\nCaught exception: ", e.what()),
                job_id
            );
        } else {
            stmt.bind_and_execute(
                EnumVal(Job::Status::FAILED), concat("\nCaught exception: ", e.what()), job_id
            );
        }

        transaction.commit();
    }
}

} // namespace job_server
