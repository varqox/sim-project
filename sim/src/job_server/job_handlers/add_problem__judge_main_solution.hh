#pragma once

#include "sim/jobs/job.hh"
#include "src/job_server/job_handlers/add_or_reupload_problem__judge_main_solution_base.hh"

namespace job_server::job_handlers {

class AddProblemJudgeModelSolution final : public AddOrReuploadProblemJudgeModelSolutionBase {
public:
    AddProblemJudgeModelSolution(uint64_t job_id, StringView job_creator,
            const sim::jobs::AddProblemInfo& info, uint64_t job_file_id,
            std::optional<uint64_t> tmp_file_id)
    : JobHandler(job_id)
    , AddOrReuploadProblemJudgeModelSolutionBase(job_id,
              sim::jobs::Job::Type::ADD_PROBLEM__JUDGE_MODEL_SOLUTION, job_creator, info,
              job_file_id, tmp_file_id, std::nullopt) {}
};

} // namespace job_server::job_handlers
