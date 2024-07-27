#pragma once

#include <sim/mysql/mysql.hh>
#include <simlib/string_view.hh>

namespace sim::jobs {

void restart_job(mysql::Connection& mysql, StringView job_id, bool notify_job_server);

} // namespace sim::jobs
