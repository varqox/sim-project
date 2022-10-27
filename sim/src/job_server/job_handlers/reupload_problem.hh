#pragma once

#include "add_or_reupload_problem_base.hh"

#include <sim/jobs/job.hh>

namespace job_server::job_handlers {

class ReuploadProblem final : public AddOrReuploadProblemBase {
public:
    ReuploadProblem(uint64_t job_id, StringView job_creator, const sim::jobs::AddProblemInfo& info,
            uint64_t job_file_id, std::optional<uint64_t> tmp_file_id, uint64_t problem_id)
    : JobHandler(job_id)
    , AddOrReuploadProblemBase(sim::jobs::Job::Type::REUPLOAD_PROBLEM, job_creator, info,
              job_file_id, tmp_file_id, problem_id) {}

    void run() override;
};

} // namespace job_server::job_handlers
