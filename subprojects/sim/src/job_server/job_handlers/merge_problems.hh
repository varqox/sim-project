#pragma once

#include "common.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problems/problem.hh>

namespace job_server::job_handlers {

void merge_problems(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::problems::Problem::id) donor_problem_id,
    decltype(sim::problems::Problem::id) target_problem_id
);

} // namespace job_server::job_handlers
