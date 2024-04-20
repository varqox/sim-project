#pragma once

#include "add_or_reupload_problem__judge_main_solution_base.hh"

#include <sim/jobs/old_job.hh>

namespace job_server::job_handlers {

class ReuploadProblemJudgeModelSolution final : public AddOrReuploadProblemJudgeModelSolutionBase {
public:
    ReuploadProblemJudgeModelSolution(
        sim::mysql::Connection& mysql,
        uint64_t job_id,
        StringView job_creator,
        const sim::jobs::AddProblemInfo& info,
        uint64_t job_file_id,
        std::optional<uint64_t> tmp_file_id,
        uint64_t problem_id
    )
    : JobHandler(job_id)
    , AddOrReuploadProblemJudgeModelSolutionBase(
          mysql,
          job_id,
          sim::jobs::OldJob::Type::REUPLOAD_PROBLEM__JUDGE_MODEL_SOLUTION,
          job_creator,
          info,
          job_file_id,
          tmp_file_id,
          problem_id
      ) {}
};

} // namespace job_server::job_handlers
