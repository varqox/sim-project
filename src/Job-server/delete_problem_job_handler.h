#pragma once

#include "job_handler.h"

class DeleteProblemJobHandler final : public JobHandler {
	uint64_t problem_id;

public:
	DeleteProblemJobHandler(uint64_t job_id, uint64_t problem_id) : JobHandler(job_id), problem_id(problem_id) {}

	void run() override final;
};
