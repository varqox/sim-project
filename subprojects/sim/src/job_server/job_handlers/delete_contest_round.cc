#include "common.hh"
#include "delete_contest_round.hh"

#include <sim/contest_rounds/contest_round.hh>
#include <sim/contests/contest.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::contest_rounds::ContestRound;
using sim::contests::Contest;
using sim::jobs::Job;
using sim::sql::DeleteFrom;
using sim::sql::InsertInto;
using sim::sql::Select;

namespace job_server::job_handlers {

void delete_contest_round(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(ContestRound::id) contest_round_id
) {
    STACK_UNWINDING_MARK;
    auto transaction = mysql.start_repeatable_read_transaction();

    // Log some info about the deleted contest round
    {
        auto stmt = mysql.execute(Select("c.id, c.name, cr.name")
                                      .from("contest_rounds cr")
                                      .inner_join("contests c")
                                      .on("c.id=cr.contest_id")
                                      .where("cr.id=?", contest_round_id));
        decltype(Contest::id) contest_id;
        decltype(Contest::name) contest_name;
        decltype(ContestRound::name) contest_round_name;
        stmt.res_bind(contest_id, contest_name, contest_round_name);
        if (!stmt.next()) {
            logger("The contest round with id: ", contest_round_id, " does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }

        logger("Contest: ", contest_name, " (id: ", contest_id, ")");
        logger("Contest round: ", contest_round_name, " (id: ", contest_round_id, ")");
    }

    // Add jobs to delete submission files
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          utc_mysql_datetime(),
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("submissions")
                      .where("contest_round_id=?", contest_round_id));

    // Delete contest (all necessary actions will take place thanks to foreign key constraints)
    mysql.execute(DeleteFrom("contest_rounds").where("id=?", contest_round_id));

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
