#pragma once

#include "add_or_reupload_problem__judge_model_solution_base.h"

namespace job_handlers {

class AddProblemJudgeModelSolution final
   : public AddOrReuploadProblemJudgeModelSolutionBase {
public:
	AddProblemJudgeModelSolution(uint64_t job_id, StringView job_creator,
	                             const jobs::AddProblemInfo& info,
	                             uint64_t job_file_id,
	                             Optional<uint64_t> tmp_file_id)
	   : JobHandler(job_id),
	     AddOrReuploadProblemJudgeModelSolutionBase(
	        job_id, JobType::ADD_PROBLEM__JUDGE_MODEL_SOLUTION, job_creator,
	        info, job_file_id, tmp_file_id, std::nullopt) {}
};

} // namespace job_handlers
