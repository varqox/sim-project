#pragma once

#include "job_handler.h"

namespace job_handlers {

class DeleteContestProblem final : public JobHandler {
	uint64_t contest_problem_id_;

public:
	DeleteContestProblem(uint64_t job_id, uint64_t contest_problem_id)
	   : JobHandler(job_id), contest_problem_id_(contest_problem_id) {}

	void run() override final;
};

} // namespace job_handlers
