#pragma once

#include "common.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/users/user.hh>

namespace job_server::job_handlers {

void delete_user(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::users::User::id) user_id
);

} // namespace job_server::job_handlers
