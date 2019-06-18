#pragma once

#include "problem_add_or_reupload_job_handler_base.h"
#include "problem_package_reset_limits_job_handler_base.h"

class ProblemAddOrReuploadJudgeModelSolutionJobHandlerBase
   : public ProblemPackageResetLimitsJobHandlerBase,
     public ProblemAddOrReuploadJobHandlerBase {
protected:
	ProblemAddOrReuploadJudgeModelSolutionJobHandlerBase(
	   uint64_t job_id, JobType job_type, StringView job_creator,
	   const jobs::AddProblemInfo& info, uint64_t job_file_id,
	   Optional<uint64_t> tmp_file_id, Optional<uint64_t> problem_id)
	   : JobHandler(job_id),
	     ProblemAddOrReuploadJobHandlerBase(
	        job_type, job_creator, info, job_file_id, tmp_file_id, problem_id) {
	}

public:
	void run() override final;

	virtual ~ProblemAddOrReuploadJudgeModelSolutionJobHandlerBase() = default;
};
