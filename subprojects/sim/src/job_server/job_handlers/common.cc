#include "common.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/sql/sql.hh>

using sim::jobs::Job;
using sim::mysql::Connection;
using sim::sql::Update;

namespace {

void set_job_status_and_log(
    Connection& mysql,
    const job_server::job_handlers::Logger& logger,
    decltype(Job::id) job_id,
    decltype(Job::status) status
) {
    mysql.execute(
        Update("jobs").set("status=?, log=?", status, logger.get_logs()).where("id=?", job_id)
    );
}

} // namespace

namespace job_server::job_handlers {

void mark_job_as_done(Connection& mysql, const Logger& logger, decltype(Job::id) job_id) {
    set_job_status_and_log(mysql, logger, job_id, Job::Status::DONE);
}

void mark_job_as_cancelled(Connection& mysql, const Logger& logger, decltype(Job::id) job_id) {
    set_job_status_and_log(mysql, logger, job_id, Job::Status::CANCELLED);
}

void mark_job_as_failed(Connection& mysql, const Logger& logger, decltype(Job::id) job_id) {
    set_job_status_and_log(mysql, logger, job_id, Job::Status::FAILED);
}

void update_job_log(Connection& mysql, const Logger& logger, decltype(Job::id) job_id) {
    mysql.execute(Update("jobs").set("log=?", logger.get_logs()).where("id=?", job_id));
}

} // namespace job_server::job_handlers
