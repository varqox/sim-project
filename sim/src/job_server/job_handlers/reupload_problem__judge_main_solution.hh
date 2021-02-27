#pragma once

#include "src/job_server/job_handlers/add_or_reupload_problem__judge_main_solution_base.hh"

namespace job_server::job_handlers {

class ReuploadProblemJudgeModelSolution final
: public AddOrReuploadProblemJudgeModelSolutionBase {
public:
    ReuploadProblemJudgeModelSolution(
        uint64_t job_id, StringView job_creator, const sim::jobs::AddProblemInfo& info,
        uint64_t job_file_id, std::optional<uint64_t> tmp_file_id, uint64_t problem_id)
    : JobHandler(job_id)
    , AddOrReuploadProblemJudgeModelSolutionBase(
          job_id, sim::JobType::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION, job_creator, info,
          job_file_id, tmp_file_id, problem_id) {}
};

} // namespace job_server::job_handlers
