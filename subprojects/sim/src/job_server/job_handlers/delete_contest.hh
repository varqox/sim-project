#pragma once

#include "common.hh"

#include <sim/contests/contest.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>

namespace job_server::job_handlers {

void delete_contest(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::contests::Contest::id) contest_id
);

} // namespace job_server::job_handlers
