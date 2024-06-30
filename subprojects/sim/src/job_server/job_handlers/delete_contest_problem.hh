#pragma once

#include "common.hh"

#include <sim/contest_problems/contest_problem.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>

namespace job_server::job_handlers {

void delete_contest_problem(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::contest_problems::ContestProblem::id) contest_problem_id
);

} // namespace job_server::job_handlers
