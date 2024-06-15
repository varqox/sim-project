#include "delete_contest.hh"

#include <sim/jobs/old_job.hh>
#include <sim/old_mysql/old_mysql.hh>
#include <simlib/time.hh>

using sim::jobs::OldJob;

namespace job_server::job_handlers {

void DeleteContest::run(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto transaction = mysql.start_repeatable_read_transaction();

    // Log some info about the deleted contest
    {
        auto old_mysql = old_mysql::ConnectionView{mysql};
        auto stmt = old_mysql.prepare("SELECT name FROM contests WHERE id=?");
        stmt.bind_and_execute(contest_id_);
        InplaceBuff<32> cname;
        stmt.res_bind_all(cname);
        if (not stmt.next()) {
            return set_failure("Contest with id: ", contest_id_, " does not exist");
        }

        job_log("Contest: ", cname, " (", contest_id_, ")");
    }

    auto old_mysql = old_mysql::ConnectionView{mysql};
    // Add jobs to delete submission files
    old_mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " created_at, aux_id, data)"
                 " SELECT file_id, NULL, ?, ?, ?, ?, NULL, ''"
                 " FROM submissions WHERE contest_id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            utc_mysql_datetime(),
            contest_id_
        );

    // Add jobs to delete contest files
    old_mysql
        .prepare("INSERT INTO jobs(file_id, creator, type, priority, status,"
                 " created_at, aux_id, data)"
                 " SELECT file_id, NULL, ?, ?, ?, ?, NULL, ''"
                 " FROM contest_files WHERE contest_id=?")
        .bind_and_execute(
            EnumVal(OldJob::Type::DELETE_FILE),
            default_priority(OldJob::Type::DELETE_FILE),
            EnumVal(OldJob::Status::PENDING),
            utc_mysql_datetime(),
            contest_id_
        );

    // Delete contest (all necessary actions will take place thanks to foreign
    // key constrains)
    old_mysql.prepare("DELETE FROM contests WHERE id=?").bind_and_execute(contest_id_);

    job_done(mysql);

    transaction.commit();
}

} // namespace job_server::job_handlers
