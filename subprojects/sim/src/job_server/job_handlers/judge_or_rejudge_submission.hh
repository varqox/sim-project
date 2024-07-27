#pragma once

#include "common.hh"

#include <sim/jobs/job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/submissions/submission.hh>

namespace job_server::job_handlers {

void judge_or_rejudge_submission(
    sim::mysql::Connection& mysql,
    Logger& logger,
    decltype(sim::jobs::Job::id) job_id,
    decltype(sim::submissions::Submission::id) submission_id
);

} // namespace job_server::job_handlers
