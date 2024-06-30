#pragma once

#include "common.hh"

#include <sim/internal_files/internal_file.hh>
#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>

namespace job_server::job_handlers {

void delete_internal_file(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::internal_files::InternalFile::id) internal_file_id
);

} // namespace job_server::job_handlers
