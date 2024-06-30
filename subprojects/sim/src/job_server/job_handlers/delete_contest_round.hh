#pragma once

#include "common.hh"

#include <sim/contest_rounds/contest_round.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>

namespace job_server::job_handlers {

void delete_contest_round(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::contest_rounds::ContestRound::id) contest_round_id
);

} // namespace job_server::job_handlers
