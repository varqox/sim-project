#include <sim/job_server/notify.hh>
#include <sim/jobs/job.hh>
#include <sim/jobs/utils.hh>
#include <sim/sql/sql.hh>
#include <utime.h>

namespace sim::jobs {

void restart_job(mysql::Connection& mysql, StringView job_id, bool notify_job_server) {
    STACK_UNWINDING_MARK;

    auto stmt = mysql.execute(
        sql::Update("jobs").set("status=?", Job::Status::PENDING).where("id=?", job_id)
    );

    if (notify_job_server) {
        job_server::notify_job_server();
    }
}

} // namespace sim::jobs
