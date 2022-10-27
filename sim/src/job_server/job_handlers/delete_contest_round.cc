#include "delete_contest_round.hh"
#include "../main.hh"

#include <sim/jobs/job.hh>

using sim::jobs::Job;

namespace job_server::job_handlers {

void DeleteContestRound::run() {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_transaction();

    // Log some info about the deleted contest round
    {
        auto stmt = mysql.prepare("SELECT c.name, c.id, r.name"
                                  " FROM contest_rounds r"
                                  " JOIN contests c ON c.id=r.contest_id"
                                  " WHERE r.id=?");
        stmt.bind_and_execute(contest_round_id_);
        InplaceBuff<32> cname;
        InplaceBuff<32> cid;
        InplaceBuff<32> rname;
        stmt.res_bind_all(cname, cid, rname);
        if (not stmt.next()) {
            return set_failure("Contest round with id: ", contest_round_id_,
                    " does not exist or the contest hierarchy is "
                    "broken (likely the former).");
        }

        job_log("Contest: ", cname, " (", cid, ')');
        job_log("Contest round: ", rname, " (", contest_round_id_, ')');
    }

    // Add jobs to delete submission files
    mysql.prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                  " added, aux_id, info, data) "
                  "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', ''"
                  " FROM submissions WHERE contest_round_id=?")
            .bind_and_execute(EnumVal(Job::Type::DELETE_FILE),
                    default_priority(Job::Type::DELETE_FILE), EnumVal(Job::Status::PENDING),
                    mysql_date(), contest_round_id_);

    // Delete contest round (all necessary actions will take place thanks to
    // foreign key constrains)
    mysql.prepare("DELETE FROM contest_rounds WHERE id=?").bind_and_execute(contest_round_id_);

    job_done();

    transaction.commit();
}

} // namespace job_server::job_handlers
