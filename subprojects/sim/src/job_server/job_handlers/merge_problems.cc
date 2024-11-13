#include "common.hh"
#include "merge_problems.hh"

#include <sim/jobs/job.hh>
#include <sim/merge_problems_jobs/merge_problems_job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/throw_assert.hh>
#include <simlib/time.hh>

using sim::jobs::Job;
using sim::merge_problems_jobs::MergeProblemsJob;
using sim::problems::Problem;
using sim::sql::DeleteFrom;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::sql::Update;
using sim::submissions::Submission;

namespace job_server::job_handlers {

void merge_problems(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(Problem::id) donor_problem_id,
    decltype(Problem::id) target_problem_id
) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    decltype(MergeProblemsJob::rejudge_transferred_submissions) rejudge_transferred_submissions;
    {
        auto stmt = mysql.execute(Select("rejudge_transferred_submissions")
                                      .from("merge_problems_jobs")
                                      .where("id=?", job_id));
        stmt.res_bind(rejudge_transferred_submissions);
        throw_assert(stmt.next());
    }

    // Assure that both problems exist
    {
        auto stmt =
            mysql.execute(Select("simfile").from("problems").where("id=?", donor_problem_id));
        decltype(Problem::simfile) donor_problem_simfile;
        stmt.res_bind(donor_problem_simfile);
        if (!stmt.next()) {
            logger("Problem to delete does not exist");
            mark_job_as_failed(mysql, logger, job_id);
            transaction.commit();
            return;
        }

        logger("Simfile of the donor problem:\n", donor_problem_simfile);
    }
    {
        auto stmt = mysql.execute(Select("1").from("problems").where("id=?", target_problem_id));
        int x;
        stmt.res_bind(x);
        if (!stmt.next()) {
            logger("Target problem does not exist");
            mark_job_as_failed(mysql, logger, job_id);
            transaction.commit();
            return;
        }
    }

    // Transfer contest problems
    mysql.execute(Update("contest_problems")
                      .set("problem_id=?", target_problem_id)
                      .where("problem_id=?", donor_problem_id));

    auto current_utc_datetime = utc_mysql_datetime();

    // Add job to delete problem file
    mysql.execute(InsertInto("jobs(creator, type, priority, status, created_at, aux_id)")
                      .select(
                          "NULL, ?, ?, ?, ?, file_id",
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING,
                          current_utc_datetime
                      )
                      .from("problems")
                      .where("id=?", donor_problem_id));

    // Add jobs to delete problem solutions' files
    mysql.execute(
        InsertInto("jobs(creator, type, priority, status, created_at, aux_id)")
            .select(
                "NULL, ?, ?, ?, ?, file_id",
                Job::Type::DELETE_INTERNAL_FILE,
                default_priority(Job::Type::DELETE_INTERNAL_FILE),
                Job::Status::PENDING,
                current_utc_datetime
            )
            .from("submissions")
            .where("problem_id=? AND type=?", donor_problem_id, Submission::Type::PROBLEM_SOLUTION)
    );

    // Delete problem solutions
    mysql.execute(
        DeleteFrom("submissions")
            .where("problem_id=? AND type=?", donor_problem_id, Submission::Type::PROBLEM_SOLUTION)
    );

    // Collect update finals
    struct FinalToUpdate {
        decltype(Submission::user_id) user_id = 0;
        decltype(Submission::contest_problem_id) contest_problem_id;
    };

    std::vector<FinalToUpdate> finals_to_update;
    {
        auto stmt = mysql.execute(Select("DISTINCT user_id, contest_problem_id")
                                      .from("submissions")
                                      .where("problem_id=?", donor_problem_id));
        FinalToUpdate ftu;
        stmt.res_bind(ftu.user_id, ftu.contest_problem_id);
        while (stmt.next()) {
            finals_to_update.emplace_back(ftu);
        }
    }

    // Schedule rejudge of the transferred submissions
    if (rejudge_transferred_submissions) {
        mysql.execute(InsertInto("jobs (creator, type, priority, status, created_at, aux_id)")
                          .select(
                              "NULL, ?, ?, ?, ?, id",
                              Job::Type::REJUDGE_SUBMISSION,
                              default_priority(Job::Type::REJUDGE_SUBMISSION),
                              Job::Status::PENDING,
                              current_utc_datetime
                          )
                          .from("submissions")
                          .where("problem_id=?", donor_problem_id)
                          .order_by("id"));
    }

    // Transfer problem submissions that are not problem solutions
    mysql.execute(Update("submissions")
                      .set("problem_id=?", target_problem_id)
                      .where("problem_id=?", donor_problem_id));

    // Update finals (both contest and problem finals are being taken care of)
    for (const auto& ftu : finals_to_update) {
        sim::submissions::update_final(
            mysql, ftu.user_id, target_problem_id, ftu.contest_problem_id
        );
    }

    // Transfer problem tags (duplicates will not be transferred - they will be
    // deleted on problem deletion thanks to foreign key constraints)
    mysql.execute(Update("IGNORE problem_tags")
                      .set("problem_id=?", target_problem_id)
                      .where("problem_id=?", donor_problem_id));

    // Finally, delete the donor problem (solution and remaining tags will be
    // deleted automatically thanks to foreign key constraints)
    mysql.execute(DeleteFrom("problems").where("id=?", donor_problem_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
