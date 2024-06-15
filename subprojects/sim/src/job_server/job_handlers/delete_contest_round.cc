#include "delete_contest_round.hh"

#include <sim/jobs/old_job.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <simlib/time.hh>

using sim::jobs::OldJob;

namespace job_server::job_handlers {

void DeleteContestRound::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    // Log some info about the deleted contest round
    {
        auto old_mysql = old_mysql::ConnectionView{mysql};
        auto stmt = old_mysql.prepare("SELECT c.name, c.id, r.name"
                                      " FROM contest_rounds r"
                                      " JOIN contests c ON c.id=r.contest_id"
                                      " WHERE r.id=?");
        stmt.bind_and_execute(contest_round_id_);
        InplaceBuff<32> cname;
        InplaceBuff<32> cid;
        InplaceBuff<32> rname;
        stmt.res_bind_all(cname, cid, rname);
        if (not stmt.next()) {
            return set_failure(
                "Contest round with id: ",
                contest_round_id_,
                " does not exist or the contest hierarchy is "
                "broken (likely the former)."
            );
        }

        job_log("Contest: ", cname, " (", cid, ')');
        job_log("Contest round: ", rname, " (", contest_round_id_, ')');
    }

    // Add jobs to delete submission files
    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " created_at, aux_id, data) "
                 "SELECT file_id, NULL, ?, ?, ?, ?, NULL, ''"
                 " FROM submissions WHERE contest_round_id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            utc_mysql_datetime(),
            contest_round_id_
        );

    // Delete contest round (all necessary actions will take place thanks to
    // foreign key constrains)
    old_mysql.prepare("DELETE FROM contest_rounds WHERE id=?").bind_and_execute(contest_round_id_);

    job_done(mysql);

    transaction.commit();
}

} // namespace job_server::job_handlers
