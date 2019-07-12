#pragma once

#include "job_handler.h"

namespace job_handlers {

class ReselectFinalSubmissionsInContestProblem final : public JobHandler {
	uint64_t contest_problem_id;

public:
	ReselectFinalSubmissionsInContestProblem(uint64_t job_id,
	                                         uint64_t contest_problem_id)
	   : JobHandler(job_id), contest_problem_id(contest_problem_id) {}

	void run() override final;
};

} // namespace job_handlers
