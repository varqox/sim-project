#pragma once

#include <sim/mysql/mysql.hh>
#include <simlib/string_view.hh>

namespace sim::jobs {

void restart_job(mysql::Connection& mysql, StringView job_id, bool notify_job_server);

// Notifies the Job server that there are jobs to do
void notify_job_server() noexcept;

} // namespace sim::jobs
