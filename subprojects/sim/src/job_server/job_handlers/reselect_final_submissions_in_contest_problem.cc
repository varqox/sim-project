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
using sim::sql::Update;
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

    decltype(ContestProblem::score_revealing) score_revealing;
    {
        auto stmt = mysql.execute(Select("score_revealing")
                                      .from("contest_problems")
                                      .where("id=? FOR UPDATE", contest_problem_id));
        stmt.res_bind(score_revealing);
        if (!stmt.next()) {
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }
    }
    {
        auto stmt =
            mysql.execute(Select("id, type, initial_status, full_status, score")
                              .from("submissions")
                              .where("contest_problem_id=? FOR UPDATE", contest_problem_id));
        decltype(Submission::id) submission_id;
        decltype(Submission::type) type;
        decltype(Submission::initial_status) initial_status;
        decltype(Submission::full_status) full_status;
        decltype(Submission::score) score;
        stmt.res_bind(submission_id, type, initial_status, full_status, score);
        while (stmt.next()) {
            mysql.execute(Update("submissions")
                              .set(
                                  "initial_final_candidate=?",
                                  sim::submissions::is_initial_final_candidate(
                                      type, initial_status, full_status, score, score_revealing
                                  )
                              )
                              .where("id=?", submission_id));
        }
    }

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
