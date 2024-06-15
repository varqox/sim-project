#include "job_handler.hh"

#include <sim/jobs/old_job.hh>
#include <sim/old_mysql/old_mysql.hh>

using sim::jobs::OldJob;

namespace job_server::job_handlers {

void JobHandler::job_canceled(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("UPDATE jobs SET status=?, log=? WHERE id=?")
        .bind_and_execute(EnumVal(OldJob::Status::CANCELED), get_log(), job_id_);
}

void JobHandler::job_done(sim::mysql::Connection& mysql) {
    STACK_UNWINDING_MARK;

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("UPDATE jobs SET status=?, log=? WHERE id=?")
        .bind_and_execute(EnumVal(OldJob::Status::DONE), get_log(), job_id_);
}

} // namespace job_server::job_handlers
