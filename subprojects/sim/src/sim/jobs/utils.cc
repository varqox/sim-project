#include "../../job_server/notify_file.hh"

#include <sim/jobs/utils.hh>
#include <simlib/time.hh>
#include <utime.h>

namespace sim::jobs {

void restart_job(
    mysql::Connection& mysql,
    StringView job_id,
    OldJob::Type job_type,
    StringView job_info,
    bool notify_job_server
) {
    STACK_UNWINDING_MARK;
    using JT = OldJob::Type;

    // Restart reuploading problem
    bool reupload =
        is_one_of(job_type, JT::REUPLOAD_PROBLEM, JT::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION);

    if (reupload) {
        AddProblemInfo info{job_info};
        info.stage = AddProblemInfo::FIRST;

        // TODO: enclosing transaction

        // Delete temporary files created during problem adding
        auto old_mysql = old_mysql::ConnectionView{mysql};
        old_mysql
            .prepare("INSERT INTO jobs(file_id, creator, type, priority,"
                     " status, created_at, aux_id, info, data) "
                     "SELECT tmp_file_id, NULL, ?, ?, ?, ?, NULL, '', '' "
                     "FROM jobs "
                     "WHERE id=? AND tmp_file_id IS NOT NULL")
            .bind_and_execute(
                EnumVal(OldJob::Type::DELETE_FILE),
                default_priority(OldJob::Type::DELETE_FILE),
                EnumVal(OldJob::Status::PENDING),
                mysql_date(),
                job_id
            );

        // Restart job
        old_mysql
            .prepare("UPDATE jobs SET type=?, status=?, tmp_file_id=NULL, info=? "
                     "WHERE id=?")
            .bind_and_execute(
                EnumVal(JT::REUPLOAD_PROBLEM), EnumVal(OldJob::Status::PENDING), info.dump(), job_id
            );

    } else {
        // Restart job of other type

        auto old_mysql = old_mysql::ConnectionView{mysql};
        old_mysql.prepare("UPDATE jobs SET status=? WHERE id=?")
            .bind_and_execute(EnumVal(OldJob::Status::PENDING), job_id);
    }

    if (notify_job_server) {
        jobs::notify_job_server();
    }
}

void restart_job(mysql::Connection& mysql, StringView job_id, bool notify_job_server) {
    uint8_t jtype = 0;
    InplaceBuff<128> jinfo;
    auto old_mysql = old_mysql::ConnectionView{mysql};
    auto stmt = old_mysql.prepare("SELECT type, info FROM jobs WHERE id=?");
    stmt.res_bind_all(jtype, jinfo);
    stmt.bind_and_execute(job_id);
    if (stmt.next()) {
        restart_job(mysql, job_id, OldJob::Type(jtype), jinfo, notify_job_server);
    }
}

void notify_job_server() noexcept { utime(job_server::notify_file.data(), nullptr); }

} // namespace sim::jobs
