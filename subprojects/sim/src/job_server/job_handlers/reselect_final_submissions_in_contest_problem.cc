#include "reselect_final_submissions_in_contest_problem.hh"

#include <sim/submissions/update_final.hh>

namespace job_server::job_handlers {

void ReselectFinalSubmissionsInContestProblem::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();
    auto old_mysql = old_mysql::ConnectionView{mysql};

    auto stmt =
        old_mysql.prepare("SELECT DISTINCT owner, problem_id "
                          "FROM submissions WHERE contest_problem_id=? AND final_candidate=1 "
                          "ORDER by id"); // Order is important so that if the two such jobs are
                                          // running, one would block until the other finishes
    stmt.bind_and_execute(contest_problem_id_);

    old_mysql::Optional<uint64_t> sowner;
    uint64_t problem_id = 0;
    stmt.res_bind_all(sowner, problem_id);
    while (stmt.next()) {
        sim::submissions::update_final_lock(mysql, sowner, problem_id);
        sim::submissions::update_final(mysql, sowner, problem_id, contest_problem_id_, false);
    }

    job_done(mysql);

    transaction.commit();
}

} // namespace job_server::job_handlers
