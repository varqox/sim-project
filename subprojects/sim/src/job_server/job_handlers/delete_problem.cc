#include "common.hh"
#include "delete_problem.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/time.hh>

using sim::jobs::Job;
using sim::problems::Problem;
using sim::sql::DeleteFrom;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace job_server::job_handlers {

void delete_problem(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(Problem::id) problem_id
) {
    STACK_UNWINDING_MARK;
    auto transaction = mysql.start_repeatable_read_transaction();

    // Check whether the problem is not used as a contest problem
    {
        auto stmt = mysql.execute(
            Select("1").from("contest_problems").where("problem_id=?", problem_id).limit("1")
        );
        int x;
        stmt.res_bind(x);
        if (stmt.next()) {
            logger("There exists a contest problem that uses (attaches) this problem. You have to "
                   "delete all of them to be able to delete this problem.");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }
    }

    // Assure that problem exist and log its Simfile
    {
        auto stmt = mysql.execute(Select("simfile").from("problems").where("id=?", problem_id));
        decltype(Problem::simfile) simfile;
        stmt.res_bind(simfile);
        if (!stmt.next()) {
            logger("The problem does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }

        logger("Deleted problem's Simfile:\n", simfile);
    }

    auto current_datetime = utc_mysql_datetime();
    // Add job to delete problem file
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          current_datetime,
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("problems")
                      .where("id=?", problem_id));

    // Add jobs to delete problem submissions' files
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          current_datetime,
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("submissions")
                      .where("problem_id=?", problem_id));

    // Delete problem (all necessary actions will take plate thanks to foreign key constraints)
    mysql.execute(DeleteFrom("problems").where("id=?", problem_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
