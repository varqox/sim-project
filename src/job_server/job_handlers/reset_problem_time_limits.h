#pragma once

#include "reset_time_limits_in_problem_package_base.h"

namespace job_handlers {

class ResetProblemTimeLimits final
   : public ResetTimeLimitsInProblemPackageBase {
protected:
	uint64_t problem_id;

public:
	ResetProblemTimeLimits(uint64_t job_id, uint64_t problem_id)
	   : JobHandler(job_id), problem_id(problem_id) {}

	void run() override;
};

} // namespace job_handlers
