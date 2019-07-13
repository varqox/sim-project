#pragma once

#include "add_or_reupload_problem_base.h"
#include "reset_time_limits_in_problem_package_base.h"

namespace job_handlers {

class AddOrReuploadProblemJudgeModelSolutionBase
   : public ResetTimeLimitsInProblemPackageBase,
     public AddOrReuploadProblemBase {
protected:
	AddOrReuploadProblemJudgeModelSolutionBase(
	   uint64_t job_id, JobType job_type, StringView job_creator,
	   const jobs::AddProblemInfo& info, uint64_t job_file_id,
	   Optional<uint64_t> tmp_file_id, Optional<uint64_t> problem_id)
	   : JobHandler(job_id),
	     AddOrReuploadProblemBase(job_type, job_creator, info, job_file_id,
	                              tmp_file_id, problem_id) {}

public:
	void run() override final;

	virtual ~AddOrReuploadProblemJudgeModelSolutionBase() = default;
};

} // namespace job_handlers
