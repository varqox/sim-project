#pragma once

#include "job_handler.h"

namespace job_handlers {

class DeleteProblem final : public JobHandler {
	uint64_t problem_id_;

public:
	DeleteProblem(uint64_t job_id, uint64_t problem_id)
	   : JobHandler(job_id), problem_id_(problem_id) {}

	void run() override final;
};

} // namespace job_handlers
