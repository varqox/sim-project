#pragma once

#include <sim/jobs/job.hh>

namespace sim::merge_problems_jobs {

struct MergeProblemsJob {
    decltype(jobs::Job::id) id;
    bool rejudge_transferred_submissions;
};

} // namespace sim::merge_problems_jobs
