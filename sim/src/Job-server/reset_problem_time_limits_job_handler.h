#pragma once

#include "problem_package_reset_limits_job_handler_base.h"

class ResetProblemTimeLimitsJobHandler final :
	public ProblemPackageResetLimitsJobHandlerBase {
protected:
	uint64_t problem_id;

public:
	ResetProblemTimeLimitsJobHandler(uint64_t job_id, uint64_t problem_id)
		: JobHandler(job_id), problem_id(problem_id) {}

	void run() override;
};
