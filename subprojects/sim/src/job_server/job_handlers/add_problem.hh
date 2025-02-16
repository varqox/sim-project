#pragma once

#include "common.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>

namespace job_server::job_handlers {

void
add_problem(sim::mysql::Connection& mysql, Logger& logger, decltype(sim::jobs::Job::id) job_id);

} // namespace job_server::job_handlers
