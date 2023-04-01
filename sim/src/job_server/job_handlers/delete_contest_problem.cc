#include "../main.hh"
#include "delete_contest_problem.hh"

#include <sim/jobs/job.hh>

using sim::jobs::Job;

namespace job_server::job_handlers {

void DeleteContestProblem::run() {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_transaction();

    // Log some info about the deleted contest problem
    {
        auto stmt = mysql.prepare("SELECT c.name, c.id, r.name, r.id, cp.name, p.name,"
                                  " p.id "
                                  "FROM contest_problems cp "
                                  "JOIN contest_rounds r ON r.id=cp.contest_round_id "
                                  "JOIN contests c ON c.id=cp.contest_id "
                                  "JOIN problems p ON p.id=cp.problem_id "
                                  "WHERE cp.id=?");
        stmt.bind_and_execute(contest_problem_id_);
        InplaceBuff<32> cname;
        InplaceBuff<32> cid;
        InplaceBuff<32> rname;
        InplaceBuff<32> rid;
        InplaceBuff<32> cpname;
        InplaceBuff<32> pname;
        InplaceBuff<32> pid;
        stmt.res_bind_all(cname, cid, rname, rid, cpname, pname, pid);
        if (not stmt.next()) {
            return set_failure(
                "Contest problem with id: ",
                contest_problem_id_,
                " does not exist or the contest hierarchy is broken (likely the"
                " former)."
            );
        }

        job_log("Contest: ", cname, " (", cid, ')');
        job_log("Contest round: ", rname, " (", rid, ')');
        job_log("Contest problem: ", cpname, " (", contest_problem_id_, ')');
        job_log("Attached problem: ", pname, " (", pid, ')');
    }

    // Add jobs to delete submission files
    mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " added, aux_id, info, data) "
                 "SELECT file_id, NULL, ?, ?, ?, ?, NULL, '', ''"
                 " FROM submissions WHERE contest_problem_id=?")
        .bind_and_execute(
            EnumVal(Job::Type::DELETE_FILE),
            default_priority(Job::Type::DELETE_FILE),
            EnumVal(Job::Status::PENDING),
            mysql_date(),
            contest_problem_id_
        );

    // Delete contest problem (all necessary actions will take place thanks to
    // foreign key constrains)
    mysql.prepare("DELETE FROM contest_problems WHERE id=?").bind_and_execute(contest_problem_id_);

    job_done();

    transaction.commit();
}

} // namespace job_server::job_handlers
