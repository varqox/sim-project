#pragma once

#include "problem_add_or_reupload_judge_model_solution_job_handler_base.h"

class ProblemReuploadJudgeModelSolutionJobHandler final
	: public ProblemAddOrReuploadJudgeModelSolutionJobHandlerBase {
public:
	ProblemReuploadJudgeModelSolutionJobHandler(uint64_t job_id,
			StringView job_creator, const jobs::AddProblemInfo& info,
			uint64_t job_file_id, Optional<uint64_t> tmp_file_id,
			uint64_t problem_id)
		: JobHandler(job_id), ProblemAddOrReuploadJudgeModelSolutionJobHandlerBase(
			job_id, JobType::REUPLOAD_JUDGE_MODEL_SOLUTION, job_creator, info,
			job_file_id, tmp_file_id, problem_id) {}
};
