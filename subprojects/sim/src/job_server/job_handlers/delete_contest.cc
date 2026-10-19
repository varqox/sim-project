#include "common.hh"
#include "delete_contest.hh"

#include <sim/contests/contest.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <sim/submissions/update_final.hh>
#include <simlib/macros/stack_unwinding.hh>
#include <simlib/time.hh>
#include <utility>
#include <vector>

using sim::contests::Contest;
using sim::jobs::Job;
using sim::sql::DeleteFrom;
using sim::sql::InsertInto;
using sim::sql::Select;
using sim::submissions::Submission;

namespace job_server::job_handlers {

void delete_contest(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(Job::id) job_id,
    decltype(Contest::id) contest_id
) {
    STACK_UNWINDING_MARK;
    auto transaction = mysql.start_repeatable_read_transaction();

    // Log some info about the deleted contest
    {
        auto stmt = mysql.execute(Select("name").from("contests").where("id=?", contest_id));
        decltype(Contest::name) contest_name;
        stmt.res_bind(contest_name);
        if (!stmt.next()) {
            logger("The contest with id: ", contest_id, " does not exist");
            mark_job_as_cancelled(mysql, logger, job_id);
            transaction.commit();
            return;
        }

        logger("Contest: ", contest_name, " (id: ", contest_id, ")");
    }

    auto current_datetime = utc_mysql_datetime();
    // Add jobs to delete submission files
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          current_datetime,
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("submissions")
                      .where("contest_id=?", contest_id));

    // Add jobs to delete contest files
    mysql.execute(InsertInto("jobs (created_at, creator, type, priority, status, aux_id)")
                      .select(
                          "?, NULL, ?, ?, ?, file_id",
                          current_datetime,
                          Job::Type::DELETE_INTERNAL_FILE,
                          default_priority(Job::Type::DELETE_INTERNAL_FILE),
                          Job::Status::PENDING
                      )
                      .from("contest_files")
                      .where("contest_id=?", contest_id));

    std::vector<std::pair<decltype(Submission::user_id), decltype(Submission::problem_id)>>
        finals_to_update;
    {
        auto stmt = mysql.execute(Select("DISTINCT user_id, problem_id")
                                      .from("submissions")
                                      .where("contest_id=? FOR UPDATE", contest_id));
        decltype(Submission::user_id) user_id;
        decltype(Submission::problem_id) problem_id;
        stmt.res_bind(user_id, problem_id);
        while (stmt.next()) {
            finals_to_update.emplace_back(user_id, problem_id);
        }
    }

    // Delete contest (all necessary actions will take place thanks to foreign key constraints)
    mysql.execute(DeleteFrom("contests").where("id=?", contest_id));

    for (const auto& [user_id, problem_id] : finals_to_update) {
        sim::submissions::update_final(mysql, user_id, problem_id, std::nullopt);
    }

    mark_job_as_done(mysql, logger, job_id);
    transaction.commit();
}

} // namespace job_server::job_handlers
