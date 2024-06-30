#include "common.hh"
#include "reselect_final_submissions_in_contest_problem.hh"

#include <sim/contest_problems/contest_problem.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::contest_problems::ContestProblem;
using sim::jobs::Job;
using sim::sql::Select;
using sim::submissions::Submission;

namespace job_server::job_handlers {

void reselect_final_submissions_in_contest_problem(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(ContestProblem::id) contest_problem_id
) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    auto stmt = mysql.execute(Select("user_id, problem_id")
                                  .from("submissions")
                                  .where("contest_problem_id=?", contest_problem_id)
                                  .group_by("user_id, problem_id"));
    decltype(Submission::user_id) submission_user_id;
    decltype(Submission::problem_id) submission_problem_id;
    stmt.res_bind(submission_user_id, submission_problem_id);
    while (stmt.next()) {
        sim::submissions::update_final(
            mysql, submission_user_id, submission_problem_id, contest_problem_id
        );
    }

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
