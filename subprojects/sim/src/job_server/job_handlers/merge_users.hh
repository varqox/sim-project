#pragma once

#include "common.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/users/user.hh>

namespace job_server::job_handlers {

void merge_users(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::users::User::id) donor_user_id,
    decltype(sim::users::User::id) target_user_id
);

} // namespace job_server::job_handlers
