#include "../../job_server/notify_file.hh"

#include <sim/jobs/utils.hh>
#include <simlib/time.hh>
#include <utime.h>

namespace sim::jobs {

void restart_job(mysql::Connection& mysql, StringView job_id, bool notify_job_server) {
    STACK_UNWINDING_MARK;

    auto old_mysql = old_mysql::ConnectionView{mysql};
    old_mysql.prepare("UPDATE jobs SET status=? WHERE id=?")
        .bind_and_execute(EnumVal(OldJob::Status::PENDING), job_id);

    if (notify_job_server) {
        jobs::notify_job_server();
    }
}

void notify_job_server() noexcept { utime(job_server::notify_file.data(), nullptr); }

} // namespace sim::jobs
